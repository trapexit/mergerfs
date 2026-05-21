#pragma once

#include "func_mkdir_base.hpp"

namespace Func2
{
  class MkdirLUS : public MkdirBase
  {
  public:
    MkdirLUS() {}
    ~MkdirLUS() {}
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
