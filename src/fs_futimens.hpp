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

#include "fs_stat_utils.hpp"

#include <sys/stat.h>

#ifdef __linux__
#warning "using fs_futimens_linux.hpp"
#include "fs_futimens_linux.hpp"
#elif __FreeBSD__ >= 11
#warning "using fs_futimens_freebsd_11.hpp"
#include "fs_futimens_freebsd_11.hpp"
#else
#warning "using fs_futimens_generic.hpp"
#include "fs_futimens_generic.hpp"
#endif


namespace fs
{
  static
  inline
  int
  futimens(const int          fd_,
           const struct stat &st_)
  {
    struct timespec ts[2];

    ts[0] = *fs::stat_atime(&st_);
    ts[1] = *fs::stat_mtime(&st_);

    return fs::futimens(fd_,ts);
  }
}
