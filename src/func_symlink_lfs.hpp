#pragma once

#include "func_symlink_base.hpp"

namespace Func2
{
  class SymlinkLFS : public SymlinkBase
  {
  public:
    SymlinkLFS() {}
    ~SymlinkLFS() {}
  public:
    std::string_view name() const;
  public:
    int operator()(const ugid_t    &ugid,
                   const Branches  &branches,
                   const char      *const &target,
                   const fs::path  &linkpath,
                   struct stat     *&st);
  };
}
