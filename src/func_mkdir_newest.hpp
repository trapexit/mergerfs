#pragma once

#include "func_mkdir_base.hpp"

namespace Func2
{
  class MkdirNewest : public MkdirBase
  {
  public:
    MkdirNewest() {}
    ~MkdirNewest() {}
  public:
    std::string_view name() const;
  public:
    int operator()(const ugid_t   &ugid,
                   const Branches &branches,
                   const fs::path &fusepath,
                   const mode_t   &mode,
                   const mode_t   &umask);
  };
}
