#pragma once

#include "func_rename_base.hpp"
#include "func_rename_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Rename = FuncWrapper<Func2::RenameBase,
                             Func2::RenameFactory,
                             int,
                             const Branches&,
                             const fs::path&,
                             const fs::path&>;
}
