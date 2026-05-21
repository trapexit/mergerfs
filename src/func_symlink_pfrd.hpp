#pragma once

#include "func_symlink_base.hpp"

namespace Func2
{
  class SymlinkPFRD : public SymlinkBase
  {
  public:
    SymlinkPFRD() {}
    ~SymlinkPFRD() {}
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
