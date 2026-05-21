#pragma once

#include "func_access_base.hpp"

namespace Func2
{
  class AccessFF : public AccessBase
  {
  public:
    AccessFF() {}
    ~AccessFF() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath,
                   const int       mode);
  };
}
