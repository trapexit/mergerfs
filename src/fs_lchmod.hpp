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

#include "fs_lstat.hpp"

#include <string>

#include <sys/stat.h>

#define MODE_BITS (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)


namespace fs
{
  static
  inline
  int
  lchmod(const char   *pathname_,
         const mode_t  mode_)
  {
#if defined __linux__
    return ::chmod(pathname_,mode_);
#else
    return ::lchmod(pathname_,mode_);
#endif
  }

  static
  inline
  int
  lchmod(const std::string &pathname_,
         const mode_t       mode_)
  {
    return fs::lchmod(pathname_.c_str(),mode_);
  }

  static
  inline
  int
  lchmod_check_on_error(const std::string &path_,
                        const mode_t       mode_)
  {
    int rv;

    rv = fs::lchmod(path_,mode_);
    if(rv == -1)
      {
        int error;
        struct stat st;

        error = errno;
        rv = fs::lstat(path_,&st);
        if(rv == -1)
          return -1;

        if((st.st_mode & MODE_BITS) != (mode_ & MODE_BITS))
          return (errno=error,-1);
      }

    return 0;
  }
}
