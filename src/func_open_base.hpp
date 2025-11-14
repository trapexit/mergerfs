#pragma once

#include "branches.hpp"

#include "fs_path.hpp"

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
                           const mode_t    mode) = 0;
  };
}
