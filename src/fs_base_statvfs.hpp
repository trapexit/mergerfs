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

#include "errno.hpp"
#include "fs_base_open.hpp"
#include "fs_base_close.hpp"

#include <sys/statvfs.h>

#include <string>

#ifndef O_PATH
# define O_PATH 0
#endif

namespace fs
{
  static
  inline
  int
  statvfs(const char     *path,
          struct statvfs &st)
  {
    return ::statvfs(path,&st);
  }

  static
  inline
  int
  statvfs(const std::string &path,
          struct statvfs    &st)
  {
    return fs::statvfs(path.c_str(),st);
  }

  static
  inline
  int
  fstatvfs(const int       fd_,
           struct statvfs *st_)
  {
    return ::fstatvfs(fd_,st_);
  }

  static
  inline
  int
  lstatvfs(const std::string *path_,
           struct statvfs    *st_)
  {
    int fd;
    int rv;

    fd = fs::open(*path_,O_RDONLY|O_NOFOLLOW|O_PATH);
    if(fd == -1)
      return -1;

    rv = fs::fstatvfs(fd,st_);
    if(rv == -1)
      {
        rv = errno;
        fs::close(fd);
        errno = rv;
        return -1;
      }

    fs::close(fd);

    return rv;
  }
}
