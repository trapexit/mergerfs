#pragma once

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
  static
  inline
  int
  statx(const int           dirfd_,
        const char         *pathname_,
        const int           flags_,
        const unsigned int  mask_,
        struct fuse_statx  *st_)
  {
#ifdef MERGERFS_SUPPORTED_STATX
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
    return fs::statx(dirfd_,
                     pathname_.c_str(),
                     flags_,
                     mask_,
                     st_);
  }
}
