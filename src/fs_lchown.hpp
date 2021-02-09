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

#include <unistd.h>


namespace fs
{
  static
  inline
  int
  lchown(const char  *pathname_,
         const uid_t  uid_,
         const gid_t  gid_)
  {
    return ::lchown(pathname_,uid_,gid_);
  }

  static
  inline
  int
  lchown(const std::string &pathname_,
         const uid_t        uid_,
         const gid_t        gid_)
  {
    return fs::lchown(pathname_.c_str(),uid_,gid_);
  }

  static
  inline
  int
  lchown(const char        *pathname_,
         const struct stat &st_)
  {
    return fs::lchown(pathname_,st_.st_uid,st_.st_gid);
  }

  static
  inline
  int
  lchown(const std::string &pathname_,
         const struct stat &st_)
  {
    return fs::lchown(pathname_.c_str(),st_);
  }

  static
  inline
  int
  lchown_check_on_error(const std::string &path_,
                        const struct stat &st_)
  {
    int rv;

    rv = fs::lchown(path_,st_);
    if(rv == -1)
      {
        int error;
        struct stat tmpst;

        error = errno;
        rv = fs::lstat(path_,&tmpst);
        if(rv == -1)
          return -1;

        if((st_.st_uid != tmpst.st_uid) ||
           (st_.st_gid != tmpst.st_gid))
          return (errno=error,-1);
      }

    return 0;
  }
}
