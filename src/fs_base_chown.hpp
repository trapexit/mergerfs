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

#include <string>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "fs_base_stat.hpp"

namespace fs
{
  static
  inline
  int
  chown(const std::string &path,
        const uid_t        uid,
        const gid_t        gid)
  {
    return ::chown(path.c_str(),uid,gid);
  }

  static
  inline
  int
  chown(const std::string &path,
        const struct stat &st)
  {
    return fs::chown(path,st.st_uid,st.st_gid);
  }

  static
  inline
  int
  lchown(const std::string &path,
         const uid_t        uid,
         const gid_t        gid)
  {
    return ::lchown(path.c_str(),uid,gid);
  }

  static
  inline
  int
  lchown(const std::string &path,
         const struct stat &st)
  {
    return fs::lchown(path,st.st_uid,st.st_gid);
  }

  static
  inline
  int
  fchown(const int   fd,
         const uid_t uid,
         const gid_t gid)
  {
    return ::fchown(fd,uid,gid);
  }

  static
  inline
  int
  fchown(const int          fd,
         const struct stat &st)
  {
    return fs::fchown(fd,st.st_uid,st.st_gid);
  }

  static
  inline
  int
  lchown_check_on_error(const std::string &path,
                        const struct stat &st)
  {
    int rv;

    rv = fs::lchown(path,st);
    if(rv == -1)
      {
        int error;
        struct stat tmpst;

        error = errno;
        rv = fs::lstat(path,tmpst);
        if(rv == -1)
          return -1;

        if((st.st_uid != tmpst.st_uid) ||
           (st.st_gid != tmpst.st_gid))
          return (errno=error,-1);
      }

    return 0;
  }

  static
  inline
  int
  fchown_check_on_error(const int          fd,
                        const struct stat &st)
  {
    int rv;

    rv = fs::fchown(fd,st);
    if(rv == -1)
      {
        int error;
        struct stat tmpst;

        error = errno;
        rv = fs::fstat(fd,tmpst);
        if(rv == -1)
          return -1;

        if((st.st_uid != tmpst.st_uid) ||
           (st.st_gid != tmpst.st_gid))
          return (errno=error,-1);
      }

    return 0;
  }
}
