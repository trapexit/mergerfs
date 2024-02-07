/*
  ISC License

  Copyright (c) 2023, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "ghc/filesystem.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

namespace fs
{
  static
  inline
  int
  openat(const int   dirfd_,
         const char *pathname_,
         const int   flags_)
  {
    int rv;

    rv = ::openat(dirfd_,pathname_,flags_);

    return ((rv == -1) ? -errno : rv);
  }

  static
  inline
  int
  openat(const int     dirfd_,
         const char   *pathname_,
         const int     flags_,
         const mode_t  mode_)
  {
    int rv;

    rv = ::openat(dirfd_,pathname_,flags_,mode_);

    return ((rv == -1) ? -errno : rv);
  }

  static
  inline
  int
  openat(const int                    dirfd_,
         const ghc::filesystem::path &pathname_,
         const int                    flags_)
  {
    return fs::openat(dirfd_,pathname_.string().c_str(),flags_);
  }

  static
  inline
  int
  openat(const int                    dirfd_,
         const ghc::filesystem::path &pathname_,
         const int                    flags_,
         const mode_t                 mode_)
  {
    return fs::openat(dirfd_,pathname_.string().c_str(),flags_,mode_);
  }
}
