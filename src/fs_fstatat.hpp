/*
  ISC License

  Copyright (c) 2019, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "to_neg_errno.hpp"

#include <sys/stat.h>


namespace fs
{
  static
  inline
  int
  fstatat(const int    dirfd_,
          const char  *pathname_,
          struct stat *statbuf_,
          const int    flags_)
  {
    int rv;

    rv = ::fstatat(dirfd_,
                   pathname_,
                   statbuf_,
                   flags_);

    return ::to_neg_errno(rv);
  }

  static
  inline
  int
  fstatat_nofollow(const int    dirfd_,
                   const char  *pathname_,
                   struct stat *statbuf_)
  {
    return fs::fstatat(dirfd_,
                       pathname_,
                       statbuf_,
                       AT_SYMLINK_NOFOLLOW);
  }
}
