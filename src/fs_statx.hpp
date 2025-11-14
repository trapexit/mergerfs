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

    switch(followsymlinks_)
      {
      case FollowSymlinksEnum::NEVER:
        rv = fs::statx(AT_FDCWD,pathname_,flags_|AT_SYMLINK_NOFOLLOW,mask_,st_);
        return rv;
      case FollowSymlinksEnum::DIRECTORY:
        rv = fs::statx(AT_FDCWD,pathname_,flags_|AT_SYMLINK_NOFOLLOW,mask_,st_);
        if((rv >= 0) && S_ISLNK(st_->mode))
          {
            struct fuse_statx st;

            rv = fs::statx(AT_FDCWD,pathname_,AT_SYMLINK_FOLLOW,STATX_TYPE,&st);
            if(rv < 0)
              return rv;

            if(S_ISDIR(st.mode))
              *st_ = st;
          }
        return rv;
      case FollowSymlinksEnum::REGULAR:
        rv = fs::statx(AT_FDCWD,pathname_,flags_|AT_SYMLINK_NOFOLLOW,mask_,st_);
        if((rv >= 0) && S_ISLNK(st_->mode))
          {
            struct fuse_statx st;

            rv = fs::statx(AT_FDCWD,pathname_,AT_SYMLINK_FOLLOW,STATX_TYPE,&st);
            if(rv < 0)
              return rv;

            if(S_ISREG(st.mode))
              *st_ = st;
          }
        return rv;
      case FollowSymlinksEnum::ALL:
        rv = fs::statx(AT_FDCWD,pathname_,flags_|AT_SYMLINK_FOLLOW,mask_,st_);
        if(rv < 0)
          rv = fs::statx(AT_FDCWD,pathname_,flags_|AT_SYMLINK_NOFOLLOW,mask_,st_);
        return rv;
      }

    return -ENOENT;
  }
}
