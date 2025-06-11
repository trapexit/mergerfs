#pragma once

#include "fs_path.hpp"
#include "branches.hpp"

#include "fuse.h"

#include <string_view>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Func2
{
  class GetAttrBase
  {
  public:
    GetAttrBase() {}
    ~GetAttrBase() {}

  public:
    virtual std::string_view name() const = 0;

  public:
    virtual int operator()(const Branches &branches,
                           const fs::Path &fusepath,
                           struct stat    *st,
                           fuse_timeouts_t *timeout) = 0;
  };
}
