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
  GLOBAL STATE AND OPEN FILE TRACKING
  ===================================

  This file defines the global State object that holds the open_files map,
  which tracks all currently open files in the filesystem.

  OPEN_FILE STRUCTURE
  -------------------

  The OpenFile struct represents a file that is currently open. It
  contains:

  - ref_count: Number of active opens sharing this file. When it reaches 0,
    the file is eligible for cleanup.

  - backing_id: Kernel handle for FUSE passthrough mode. INVALID_BACKING_ID
    means passthrough is not enabled for this file.

  - fi: Pointer to the canonical FileInfo object containing file metadata and
    the file descriptor used as the source for overlapping opens. Since a
    file descriptor has state associated with it, concurrent opens get their
    own duplicated fd and their own FileInfo stored only in
    fuse_file_info_t.fh. The map entry keeps the canonical FileInfo so later
    opens can duplicate it and so the nodeid can retain a shared
    passthrough backing_id.

  THREAD SAFETY
  -------------

  The open_files map uses boost::concurrent_flat_map which provides:
  - Bucket-level locking for fine-grained concurrency
  - visit() for atomic read-modify operations
  - try_emplace() for atomic insertion
  - erase_if() for conditional removal

  While there is fine-grained locking it isn't per entry and as a
  result the code for opening and releasing is more complicated since
  having file io syscalls in the critical section lambda could cause
  contention.

  LIFECYCLE STATES
  ----------------

  An OpenFile entry goes through these states:

  1. NOT IN MAP - File is not open
  2. ACTIVE (ref_count >= 1) - File is open and the canonical FileInfo is in use
  3. ERASED - The final release removed the map entry and cleanup continues
     outside the map callback

  See fuse_open.cpp and fuse_release.cpp for detailed documentation on the
  concurrency patterns and per-handle cleanup rules.
*/

#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include "fileinfo.hpp"

#include "fuse.h"

#include <atomic>
#include <functional>
#include <map>
#include <string>


class State
{
public:
  State();

public:
  struct OpenFile
  {
    OpenFile() = delete;

    OpenFile(const int        backing_id_,
             FileInfo * const fi_)
      : ref_count(1),
        backing_id(backing_id_),
        fi(fi_)
    {
    }

    // Move constructor runs during initial insertion into the
    // concurrent_flat_map before the new entry is reachable by the
    // other threads. There is no concurrent observer of the moved
    // value at this point so relaxed load is sufficient.
    OpenFile(OpenFile &&o_) noexcept
      : ref_count(o_.ref_count.load(std::memory_order_relaxed)),
        backing_id(o_.backing_id),
        fi(o_.fi)
    {
    }

    std::atomic<int> ref_count;
    int backing_id;
    FileInfo *fi;
  };

public:
  FileInfo*
  get_fi(const fuse_req_ctx_t *ctx_,
         cu64                  fh_) const
  {
    FileInfo *fi;

    fi = FileInfo::from_fh(fh_);
    if(not fi)
      open_files.cvisit(ctx_->nodeid,
                        [&](auto &v_)
                        {
                          fi = v_.second.fi;
                        });

    return fi;
  }

public:
  struct GetSet
  {
    std::function<std::string()> get;
    std::function<int(const std::string_view)> set;
    std::function<int(const std::string_view)> valid;
  };

  void set_getset(const std::string   &name,
                  const State::GetSet &gs);

  int get(const std::string &key,
          std::string       &val);
  int set(const std::string      &key,
          const std::string_view  val);
  int valid(const std::string      &key,
            const std::string_view  val);

public:
  using OpenFileMap = boost::concurrent_flat_map<u64,OpenFile>;

private:
  std::map<std::string,GetSet> _getset;

public:
  OpenFileMap open_files;
};

extern State state;
