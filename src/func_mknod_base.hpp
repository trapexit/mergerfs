#pragma once

#include "branches.hpp"
#include "fs_path.hpp"
#include "ugid.hpp"

#include <string_view>

namespace Func2
{
  class MknodBase
  {
  public:
    MknodBase() {}
    ~MknodBase() {}

  public:
    virtual std::string_view name() const = 0;

  public:
    virtual int operator()(const ugid_t   &ugid,
                           const Branches &branches,
                           const fs::path &fusepath,
                           const mode_t    mode,
                           const dev_t     dev,
                           const mode_t    umask) = 0;
  };
}
