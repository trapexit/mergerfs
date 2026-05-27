#pragma once

#include "branches.hpp"

#include "fs_path.hpp"

#include <cstdint>

namespace Func2
{
  class IoctlBase
  {
  public:
    IoctlBase() {}
    virtual ~IoctlBase() = default;

  public:
    virtual std::string_view name() const = 0;

  public:
    virtual int operator()(const Branches &branches,
                           const fs::path &fusepath,
                           const uint32_t  &cmd,
                           void            *&data,
                           uint32_t        *&out_bufsz) = 0;
  };
}
