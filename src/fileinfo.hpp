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
  // take shared lock; move takes unique.
  std::shared_mutex mutex;
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
