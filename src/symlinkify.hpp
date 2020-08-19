/*
  ISC License

  Copyright (c) 2017, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include <sys/stat.h>
#include <time.h>

namespace symlinkify
{
  static
  inline
  bool
  can_be_symlink(const struct stat &st_,
                 const time_t       timeout_)
  {
    if(S_ISDIR(st_.st_mode) ||
       (st_.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH)))
      return false;

    const time_t now = ::time(NULL);

    return (((now - st_.st_mtime) > timeout_) &&
            ((now - st_.st_ctime) > timeout_));
  }

  static
  inline
  mode_t
  convert(const mode_t mode_)
  {
    return ((mode_ & ~S_IFMT) | S_IFLNK);
  }
}
