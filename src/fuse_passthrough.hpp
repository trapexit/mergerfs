#pragma once

#include "ugid.hpp"

#include "fuse.h"

// Becoming root is required due to current security policies within
// the kernel. This may be able to be changed in the future.

namespace FUSE
{
  static
  inline
  int
  passthrough_open(const int fd_)
  {
    const ugid::SetRootGuard _;

    return fuse_passthrough_open(fd_);
  }

  static
  inline
  int
  passthrough_close(const int backing_id_)
  {
    const ugid::SetRootGuard _;

    return fuse_passthrough_close(backing_id_);
  }
}
