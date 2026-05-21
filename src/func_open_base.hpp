#pragma once

#include "branches.hpp"
#include "config_nfsopenhack.hpp"

#include "fs_path.hpp"
#include "fuse.h"

#include <sys/types.h>


namespace Func2
{
  class OpenBase
  {
  public:
    OpenBase() {}
    ~OpenBase() {}

  public:
    virtual std::string_view name() const = 0;

  public:
    virtual int operator()(const Branches &branches,
                           const fs::path &fusepath,
                           fuse_file_info_t *&ffi,
                           const bool       &link_cow,
                           const NFSOpenHack &nfsopenhack) = 0;
  };
}
