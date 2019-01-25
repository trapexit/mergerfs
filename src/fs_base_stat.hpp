/*
  ISC License

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

#include <string>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace fs
{
  static
  inline
  int
  stat(const char  *path_,
       struct stat *st_)
  {
    return ::stat(path_,st_);
  }

  static
  inline
  int
  stat(const std::string &path_,
       struct stat       *st_)
  {
    return fs::stat(path_.c_str(),st_);
  }

  static
  inline
  int
  lstat(const char  *path_,
        struct stat *st_)
  {
    return ::lstat(path_,st_);
  }

  static
  inline
  int
  lstat(const std::string &path_,
        struct stat       *st_)
  {
    return fs::lstat(path_.c_str(),st_);
  }

  static
  inline
  int
  fstat(const int    fd_,
        struct stat *st_)
  {
    return ::fstat(fd_,st_);
  }

  static
  inline
  timespec *
  stat_atime(struct stat *st_)
  {
#if __APPLE__
    return &st_->st_atimespec;
#else
    return &st_->st_atim;
#endif
  }

  static
  inline
  const
  timespec *
  stat_atime(const struct stat *st_)
  {
#if __APPLE__
    return &st_->st_atimespec;
#else
    return &st_->st_atim;
#endif
  }

  static
  inline
  timespec *
  stat_mtime(struct stat *st_)
  {
#if __APPLE__
    return &st_->st_mtimespec;
#else
    return &st_->st_mtim;
#endif
  }

  static
  inline
  const
  timespec *
  stat_mtime(const struct stat *st_)
  {
#if __APPLE__
    return &st_->st_mtimespec;
#else
    return &st_->st_mtim;
#endif
  }
}
