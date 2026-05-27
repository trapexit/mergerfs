#pragma once

#include "branches.hpp"
#include "fs_path.hpp"

namespace Func2
{
  class LinkBase
  {
  public:
    LinkBase() {}
    virtual ~LinkBase() = default;

  public:
    virtual std::string_view name() const = 0;

  public:
    virtual int operator()(const Branches &branches,
                           const fs::path &oldfusepath,
                           const fs::path &newfusepath) = 0;
  };
}
