#pragma once

#include "branches.hpp"

#include "fs_path.hpp"

namespace Func2
{
  class TruncateBase
  {
  public:
    TruncateBase() {}
    ~TruncateBase() {}

  public:
    virtual std::string_view name() const = 0;

  public:
    virtual int operator()(const Branches &branches,
                           const fs::path &fusepath,
                           const off_t     size) = 0;
  };
}
