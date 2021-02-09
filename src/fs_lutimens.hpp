/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_utimensat.hpp"
#include "fs_stat_utils.hpp"


namespace fs
{
  static
  inline
  int
  lutimens(const std::string     &path_,
           const struct timespec  ts_[2])
  {
    return fs::utimensat(AT_FDCWD,path_,ts_,AT_SYMLINK_NOFOLLOW);
  }

  static
  inline
  int
  lutimens(const std::string &path_,
           const struct stat &st_)
  {
    struct timespec ts[2];

    ts[0] = *fs::stat_atime(&st_);
    ts[1] = *fs::stat_mtime(&st_);

    return fs::lutimens(path_,ts);
  }
}
