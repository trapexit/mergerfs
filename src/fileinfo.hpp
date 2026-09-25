/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
  FILEINFO - OPEN FILE METADATA
  =============================

  This file defines the FileInfo class which stores metadata for an open
  file. Each create and opened file get its own instance while the
  first one of any set of concurrently open files will be stored in
  open_files map for reference but always pass around through
  fuse_file_info_t.fh.

  FUSE FILE HANDLE CONVERSION
  ---------------------------

  FUSE uses a 64-bit file handle (fuse_file_info_t.fh). We convert between
  FileInfo* and fh using simple pointer casting:

  - to_fh(FileInfo*): Casts pointer to uint64_t for FUSE
  - from_fh(uint64_t): Casts back to FileInfo pointer

  This is safe because FileInfo objects remain valid until the last close.

  See fuse_open.cpp and fuse_release.cpp for how FileInfo is used in the
  open/close flow.
*/

#pragma once

#include "assert.hpp"
#include "branch.hpp"
#include "fh.hpp"
#include "fs_path.hpp"

#include "base_types.h"

#include <atomic>
#include <mutex>
#include <shared_mutex>


class FileInfo : public FH
{
public:
  static FileInfo *from_fh(const u64);
  static u64       to_fh(const FileInfo*);

public:
  FileInfo(const int       fd_,
           const Branch   *branch_,
           const fs::path &fusepath_,
           const bool      direct_io_)
    : FH(fusepath_),
      fd(fd_),
      branch(*branch_),
      direct_io(direct_io_)
  {
  }

  FileInfo(const int       fd_,
           const Branch   &branch_,
           const fs::path &fusepath_,
           const bool      direct_io_)
    : FH(fusepath_),
      fd(fd_),
      branch(branch_),
      direct_io(direct_io_)
  {
  }

  FileInfo(const FileInfo *fi_)
    : FH(fi_->fusepath),
      fd(fi_->fd),
      branch(fi_->branch),
      direct_io(fi_->direct_io)
  {
  }

public:
  u64 to_fh() const;

public:
  int fd;
  Branch branch;
  u32 direct_io:1;
  // Serializes the fd state across concurrent writes on the same open
  // file. Concurrent writes happen with:
  // 1) writeback-cache + page-cache mode
  // 2) parallel_direct_writes + direct_io=true
  //
  // Common case (plain pwrite on stable fd) only needs a shared lock
  // since the kernel already serializes fd mutations. The danger is
  // the fs::dup2() rebinding when moveonenospc activates. Writers
  // take shared lock; move takes unique (lock_exclusive). Spliced
  // reads (FUSE::read_buf) also take it shared, but only if that
  // needs no waiting.
  std::shared_mutex mutex;
  // Threads waiting in lock_exclusive. FUSE::read_buf starts no splice
  // while one waits: the rwlock prefers readers, so overlapping spliced
  // reads could otherwise hold a mover off indefinitely.
  std::atomic<int> exclusive_waiters{0};
  // Lower bound on the branch file's size, used only by FUSE::read_buf
  // to skip its end-of-file size check for reads ending inside this
  // bound. Raised by that check, lowered by any short read (FUSE::read)
  // or ftruncate and reset by fallocate through this handle. It may go
  // stale when the file shrinks through another handle or outside
  // mergerfs; the splice reply then hits EOF and falls back to
  // FUSE::read, which returns the correct bytes and lowers the bound
  // to where that read found the end (to 0 when it read nothing, as
  // the end is then somewhere at or before its offset). 0 = unknown.
  std::atomic<s64> size_floor{0};
  // Whether FUSE::read_buf may offer fd for splicing: unknown until the
  // first large read checks the branch filesystem (some serve splice
  // from a page cache their reads bypass or revalidate), no once that
  // check said so or the fd refused splice (its filesystem has no
  // splice_read), after which reads go straight to the copy path.
  // Decided once per fd binding; a filesystem refused by type is also
  // remembered by device, so later handles on it are settled without
  // repeating that check (see FUSE::read_buf).
  enum class Splice : uint8_t { unknown, yes, no };
  std::atomic<Splice> splice_state{Splice::unknown};

  // Take mutex exclusively (moveonenospc), announced through
  // exclusive_waiters so that no new spliced read starts meanwhile.
  std::unique_lock<std::shared_mutex>
  lock_exclusive()
  {
    // Counted out again even if locking throws, so a failed attempt
    // cannot leave spliced reads on this handle declined for good.
    struct Waiting
    {
      std::atomic<int> &count;

      explicit Waiting(std::atomic<int> &count_)
        : count(count_)
      {
        count.fetch_add(1,std::memory_order_relaxed);
      }

      ~Waiting()
      {
        count.fetch_sub(1,std::memory_order_relaxed);
      }
    } waiting(exclusive_waiters);

    return std::unique_lock<std::shared_mutex>(mutex);
  }

  void size_floor_lower(const s64 v_)
  {
    s64 cur = size_floor.load(std::memory_order_relaxed);
    while((v_ < cur) &&
          !size_floor.compare_exchange_weak(cur,v_,std::memory_order_relaxed))
      ;
  }

  // The file's size may have changed in a way size_floor_lower cannot
  // express: call after the change. That narrows, but does not close,
  // the window in which a size check that started before the change
  // (and saw 0, the value stored here) installs the old size
  // afterwards; like any stale floor that only costs a fallback.
  void size_floor_reset()
  {
    size_floor.store(0,std::memory_order_relaxed);
  }

  // fd now refers to a different branch file (moveonenospc's dup2): a
  // splice decision or size observed on the old one no longer applies.
  void fd_rebound()
  {
    splice_state.store(Splice::unknown,std::memory_order_relaxed);
    size_floor_reset();
  }
};

inline
u64
FileInfo::to_fh(const FileInfo *fi_)
{
  return reinterpret_cast<u64>(fi_);
}

inline
u64
FileInfo::to_fh() const
{
  return to_fh(this);
}

inline
FileInfo*
FileInfo::from_fh(const u64 fh_)
{
  return reinterpret_cast<FileInfo*>(fh_);
}
