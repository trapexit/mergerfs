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

#include "fs_fstat.hpp"
#include "to_neg_errno.hpp"

#include <sys/stat.h>

#define MODE_BITS (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)


namespace fs
{
  static
  inline
  int
  fchmod(const int    fd_,
         const mode_t mode_)
  {
    int rv;

    rv = ::fchmod(fd_,mode_);

    return ::to_neg_errno(rv);
  }

  static
  inline
  int
  fchmod(const int          fd_,
         const struct stat &st_)
  {
    return fs::fchmod(fd_,st_.st_mode);
  }

  static
  inline
  int
  fchmod_check_on_error(const int          fd_,
                        const struct stat &st_)
  {
    int rv;

    rv = fs::fchmod(fd_,st_);
    if(rv < 0)
      {
        int err;
        struct stat tmpst;

        err = rv;
        rv = fs::fstat(fd_,&tmpst);
        if(rv < 0)
          return rv;

        if((st_.st_mode & MODE_BITS) != (tmpst.st_mode & MODE_BITS))
          return err;
      }

    return rv;
  }
}
