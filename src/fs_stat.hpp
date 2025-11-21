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

#include "die.hpp"
#include "casts.hpp"
#include "follow_symlinks_enum.hpp"
#include "to_neg_errno.hpp"
#include "to_cstr.hpp"
#include "fs_lstat.hpp"

#include <string>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


namespace fs
{
  template<typename PathType>
  static
  inline
  int
  stat(const PathType &path_,
       struct stat    *st_)
  {
    int rv;

    rv = ::stat(to_cstr(path_),st_);

    return ::to_neg_errno(rv);
  }

  template<typename PathType>
  static
  inline
  int
  stat(const PathType     &path_,
       struct stat        *st_,
       FollowSymlinksEnum  follow_)
  {
    int rv;

    switch(follow_)
      {
      case FollowSymlinksEnum::NEVER:
        rv = fs::lstat(path_,st_);
        return rv;
      case FollowSymlinksEnum::DIRECTORY:
        rv = fs::lstat(path_,st_);
        if((rv >= 0) && S_ISLNK(st_->st_mode))
          {
            struct stat st;

            rv = fs::stat(path_,&st);
            if(rv < 0)
              return rv;

            if(S_ISDIR(st.st_mode))
              *st_ = st;
          }
        return rv;
      case FollowSymlinksEnum::REGULAR:
        rv = fs::lstat(path_,st_);
        if((rv >= 0) && S_ISLNK(st_->st_mode))
          {
            struct stat st;

            rv = fs::stat(path_,&st);
            if(rv < 0)
              return rv;

            if(S_ISREG(st.st_mode))
              *st_ = st;
          }
        return rv;
      case FollowSymlinksEnum::ALL:
        rv = fs::stat(path_,st_);
        if(rv < 0)
          rv = fs::lstat(path_,st_);
        return rv;
      }

    auto s = fmt::format("Should never reach this: follow_={}",sc<int>(follow_));
    DIE(s);
    return -EINVAL;
  }
}
