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

  - fi: Pointer to the FileInfo object containing file metadata and
    the actual file descriptor to the **first** opened file. Since a
    specific file descriptor has state associated with it you can not
    simply reuse the FD for overlapping open requests. Instead it is
    reopened using openat(/proc/self/fd/X). Additionally, FUSE
    passthrough feature requires that the same underlying file be used
    for the nodeid and backing_id. All secondary opens are stored in
    the fuse_file_info_t.fh only. When set to nullptr, the entry is in
    "zombie" state (being destroyed but not yet removed from the map).

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
  2. ACTIVE (ref_count >= 1, fi != nullptr) - File is open and in use
  3. ZOMBIE (ref_count = 0, fi = nullptr) - File is being destroyed
  4. CLEANED UP - Resources freed, entry erased from map

  See fuse_open.cpp and fuse_release.cpp for detailed documentation on the
  concurrency patterns and two-phase cleanup mechanism.
*/

#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include "fileinfo.hpp"

#include "fuse.h"

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

    int ref_count;
    int backing_id;
    FileInfo *fi;
  };

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
