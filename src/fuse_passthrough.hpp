#pragma once

#include "ugid.hpp"

#include "fuse.h"

namespace FUSE
{
  static
  inline
  int
  passthrough_open(const fuse_context *fc_,
                   const int           fd_)
  {
    const ugid::SetRootGuard _;

    return fuse_passthrough_open(fc_,fd_);
  }

  static
  inline
  int
  passthrough_close(const fuse_context *fc_,
                    const int           backing_id_)
  {
    const ugid::SetRootGuard _;

    return fuse_passthrough_close(fc_,backing_id_);
  }
}
