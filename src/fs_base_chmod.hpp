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

#include <sys/stat.h>

#include "fs_base_stat.hpp"

#define MODE_BITS (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)

namespace fs
{
  static
  inline
  int
  chmod(const std::string &path,
        const mode_t       mode)
  {
    return ::chmod(path.c_str(),mode);
  }

  static
  inline
  int
  fchmod(const int    fd,
         const mode_t mode)
  {
    return ::fchmod(fd,mode);
  }

  static
  inline
  int
  fchmod(const int          fd,
         const struct stat &st)
  {
    return ::fchmod(fd,st.st_mode);
  }

  static
  inline
  int
  chmod_check_on_error(const std::string &path,
                       const mode_t       mode)
  {
    int rv;

    rv = fs::chmod(path,mode);
    if(rv == -1)
      {
        int error;
        struct stat st;

        error = errno;
        rv = fs::stat(path,st);
        if(rv == -1)
          return -1;

        if((st.st_mode & MODE_BITS) != (mode & MODE_BITS))
          return (errno=error,-1);
      }

    return 0;
  }

  static
  inline
  int
  fchmod_check_on_error(const int          fd,
                        const struct stat &st)
  {
    int rv;

    rv = fs::fchmod(fd,st);
    if(rv == -1)
      {
        int error;
        struct stat tmpst;

        error = errno;
        rv = fs::fstat(fd,tmpst);
        if(rv == -1)
          return -1;

        if((st.st_mode & MODE_BITS) != (tmpst.st_mode & MODE_BITS))
          return (errno=error,-1);
      }

    return 0;
  }
}
