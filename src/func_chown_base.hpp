#pragma once

#include "branches.hpp"

#include "fs_path.hpp"

#include <sys/types.h>


namespace Func2
{
  class ChownBase
  {
  public:
    ChownBase() {}
    ~ChownBase() {}

  public:
    virtual std::string_view name() const = 0;

  public:
    virtual int operator()(const Branches &branches,
                           const fs::path &fusepath,
                           const uid_t     uid,
                           const gid_t     gid) = 0;
  };
}
