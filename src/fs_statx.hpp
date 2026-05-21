/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "follow_symlinks_enum.hpp"
#include "to_cstr.hpp"
#include "to_neg_errno.hpp"

#include "fuse_kernel.h"

#include <string>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <sys/stat.h>

#include "supported_statx.hpp"


namespace fs
{
  template<typename PathType>
  static
  inline
  int
  statx(const int           dirfd_,
        const PathType     &pathname_,
        const int           flags_,
        const unsigned int  mask_,
        struct fuse_statx  *st_)
  {
#ifdef MERGERFS_SUPPORTED_STATX
    int rv;

    rv = ::statx(dirfd_,
                 to_cstr(pathname_),
                 flags_,
                 mask_,
                 (struct statx*)st_);

    return ::to_neg_errno(rv);
#else
    return -ENOSYS;
#endif
  }

  template<typename PathType>
  static
  inline
  int
  statx(const int           dirfd_,
        const PathType     &pathname_,
        const int           flags_,
        const unsigned int  mask_,
        struct fuse_statx  *st_,
        FollowSymlinksEnum  followsymlinks_)
  {
    int rv;

    // statx(2) recognises AT_SYMLINK_NOFOLLOW; follow-the-link is the
    // default when that flag is absent. AT_SYMLINK_FOLLOW (a linkat(2)
    // flag) is not meaningful here and is omitted.
    const int follow_flags   = (flags_ & ~AT_SYMLINK_NOFOLLOW);
    const int nofollow_flags = (flags_ | AT_SYMLINK_NOFOLLOW);

    switch(followsymlinks_)
      {
      case FollowSymlinksEnum::NEVER:
        rv = fs::statx(dirfd_,pathname_,nofollow_flags,mask_,st_);
        return rv;
      case FollowSymlinksEnum::DIRECTORY:
        rv = fs::statx(dirfd_,pathname_,nofollow_flags,mask_,st_);
        if((rv >= 0) && S_ISLNK(st_->mode))
          {
            struct fuse_statx st{};

            // Request the same mask as the caller: STATX_TYPE alone
            // would leave the rest of `st` unspecified and clobber the
            // caller's data when we copy back. On follow failure
            // (dangling link, etc.), keep the lstat data so callers
            // can still display the entry.
            if((fs::statx(dirfd_,pathname_,follow_flags,mask_,&st) >= 0) &&
               S_ISDIR(st.mode))
              *st_ = st;
          }
        return rv;
      case FollowSymlinksEnum::REGULAR:
        rv = fs::statx(dirfd_,pathname_,nofollow_flags,mask_,st_);
        if((rv >= 0) && S_ISLNK(st_->mode))
          {
            struct fuse_statx st{};

            if((fs::statx(dirfd_,pathname_,follow_flags,mask_,&st) >= 0) &&
               S_ISREG(st.mode))
              *st_ = st;
          }
        return rv;
      case FollowSymlinksEnum::ALL:
        rv = fs::statx(dirfd_,pathname_,follow_flags,mask_,st_);
        if(rv < 0)
          rv = fs::statx(dirfd_,pathname_,nofollow_flags,mask_,st_);
        return rv;
      }

    return -ENOENT;
  }
}
