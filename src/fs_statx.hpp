#pragma once

#include "fuse_kernel.h"

#include <string>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <sys/stat.h>

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
#ifdef STATX_TYPE
    int rv;

    rv = ::statx(dirfd_,
                 pathname_,
                 flags_,
                 mask_,
                 (struct statx*)st_);

    return ((rv == -1) ? -errno : 0);
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
}
