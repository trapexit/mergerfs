#pragma once

#include "to_neg_errno.hpp"
#include "follow_symlinks_enum.hpp"

#include "fuse_kernel.h"

#include <string>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <sys/stat.h>

#ifdef STATX_TYPE
//#define MERGERFS_STATX_SUPPORTED
#endif

namespace fs
{
  static
  inline
  int
  statx(const int           dirfd_,
        const char         *pathname_,
        const int           flags_,
        const unsigned int  mask_,
        struct fuse_statx  *st_)
  {
#ifdef MERGERFS_STATX_SUPPORTED
    int rv;

    rv = ::statx(dirfd_,
                 pathname_,
                 flags_,
                 mask_,
                 (struct statx*)st_);

    return ::to_neg_errno(rv);
#else
    return -ENOSYS;
#endif
  }

  static
  inline
  int
  statx(const int           dirfd_,
        const std::string  &pathname_,
        const int           flags_,
        const unsigned int  mask_,
        struct fuse_statx  *st_)
  {
    return fs::statx(dirfd_,pathname_.c_str(),flags_,mask_,st_);
  }

  int
  statx(const int           dirfd,
        const char         *pathname,
        const int           flags,
        const unsigned int  mask,
        struct fuse_statx  *st,
        FollowSymlinksEnum  follow);

  static
  inline
  int
  statx(const int           dirfd_,
        const std::string  &pathname_,
        const int           flags_,
        const unsigned int  mask_,
        struct fuse_statx  *st_,
        FollowSymlinksEnum  follow_)
  {
    return fs::statx(dirfd_,
                     pathname_.c_str(),
                     flags_,
                     mask_,
                     st_,
                     follow_);
  }

  static
  inline
  int
  statx(const char         *pathname_,
        const int           flags_,
        const unsigned int  mask_,
        struct fuse_statx  *st_,
        FollowSymlinksEnum  follow_)
  {
    return fs::statx(AT_FDCWD,
                     pathname_,
                     flags_,
                     mask_,
                     st_,
                     follow_);
  }

  static
  inline
  int
  statx(const std::string  &pathname_,
        const int           flags_,
        const unsigned int  mask_,
        struct fuse_statx  *st_,
        FollowSymlinksEnum  follow_)
  {
    return fs::statx(pathname_.c_str(),
                     flags_,
                     mask_,
                     st_,
                     follow_);
  }
}
