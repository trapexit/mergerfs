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

#ifndef __FS_BASE_UTIME_HPP__
#define __FS_BASE_UTIME_HPP__

#ifdef __linux__
# include "fs_base_utime_utimensat.hpp"
#elif __FreeBSD__ >= 11
# include "fs_base_utime_utimensat.hpp"
#else
# include "fs_base_utime_generic.hpp"
#endif

#include "fs_base_stat.hpp"

namespace fs
{
  static
  inline
  int
  utime(const std::string &path,
        const struct stat &st)
  {
    struct timespec times[2];

    times[0] = *fs::stat_atime(st);
    times[1] = *fs::stat_mtime(st);

    return fs::utime(AT_FDCWD,path,times,0);
  }

  static
  inline
  int
  utime(const int          fd,
        const struct stat &st)
  {
    struct timespec times[2];

    times[0] = *fs::stat_atime(st);
    times[1] = *fs::stat_mtime(st);

    return fs::utime(fd,times);
  }

  static
  inline
  int
  lutime(const std::string     &path,
         const struct timespec  times[2])
  {
    return fs::utime(AT_FDCWD,path,times,AT_SYMLINK_NOFOLLOW);
  }
}

#endif
