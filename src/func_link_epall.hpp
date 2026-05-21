#pragma once

#include "func_link_base.hpp"

namespace Func2
{
  class LinkEPAll : public LinkBase
  {
  public:
    LinkEPAll() {}
    ~LinkEPAll() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &oldfusepath,
                   const fs::path &newfusepath);
  };
}
