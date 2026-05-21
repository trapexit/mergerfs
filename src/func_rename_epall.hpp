#pragma once

#include "func_rename_base.hpp"

namespace Func2
{
  class RenameEPAll : public RenameBase
  {
  public:
    RenameEPAll() {}
    ~RenameEPAll() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &oldfusepath,
                   const fs::path &newfusepath);
  };
}
