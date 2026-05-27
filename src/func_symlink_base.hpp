#pragma once

#include "branches.hpp"
#include "fs_path.hpp"
#include "ugid.hpp"

#include <string>


namespace Func2
{
  class SymlinkBase
  {
  public:
    SymlinkBase() {}
    virtual ~SymlinkBase() = default;

  public:
    virtual std::string_view name() const = 0;

  public:
    virtual int operator()(const ugid_t    &ugid,
                           const Branches  &branches,
                           const char      *const &target,
                           const fs::path  &linkpath,
                           struct stat     *&st) = 0;
  };
}
