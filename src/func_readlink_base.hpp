#pragma once

#include "branches.hpp"

#include "fs_path.hpp"

namespace Func2
{
  class ReadlinkBase
  {
  public:
    ReadlinkBase() {}
    ~ReadlinkBase() {}

  public:
    virtual std::string_view name() const = 0;

  public:
    virtual int operator()(const Branches &branches,
                           const fs::path &fusepath,
                           char           *buf,
                           const size_t    bufsize,
                           const time_t    symlinkify_timeout) = 0;
  };
}
