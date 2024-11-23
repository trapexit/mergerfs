#pragma once

#include "fs_path.hpp"
#include "branches.hpp"

#include "fuse.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Func2
{
  class GetattrBase
  {
  public:
    GetattrBase() {}
    ~GetattrBase() {}
  public:
    virtual int process(const Branches &branches,
                        const fs::Path &fusepath,
                        struct stat *st,
                        fuse_timeouts_t *timeout) = 0;
  };
}
