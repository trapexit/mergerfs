#pragma once

#include "func_symlink_base.hpp"
#include "func_symlink_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Symlink = FuncWrapper<Func2::SymlinkBase,
                              Func2::SymlinkFactory,
                              int,
                              const ugid_t&,
                              const Branches&,
                              const char *const&,
                              const fs::path&,
                              struct stat*&>;
}
