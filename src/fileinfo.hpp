/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#pragma once

#include "branch.hpp"
#include "fh.hpp"
#include "fs_path.hpp"

#include "base_types.h"

#include <cassert>
#include <mutex>


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
  std::mutex mutex;
};

inline
u64
FileInfo::to_fh(const FileInfo *fi_)
{
  assert(fi_ != nullptr);
  return reinterpret_cast<u64>(fi_);
}

inline
u64
FileInfo::to_fh() const
{
  return FileInfo::to_fh(this);
}

inline
FileInfo*
FileInfo::from_fh(const u64 fh_)
{
  assert(fh_ != 0);
  return reinterpret_cast<FileInfo*>(fh_);
}
