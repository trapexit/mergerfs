#pragma once

#include "func_mknod_base.hpp"

namespace Func2
{
  class MknodRAND : public MknodBase
  {
  public:
    MknodRAND() {}
    ~MknodRAND() {}
  public:
    std::string_view name() const;
  public:
    int operator()(const ugid_t   &ugid,
                   const Branches &branches,
                   const fs::path &fusepath,
                   const mode_t    mode,
                   const dev_t     dev,
                   const mode_t    umask);
  };
}
