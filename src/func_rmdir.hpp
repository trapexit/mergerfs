#pragma once

#include "func_rmdir_base.hpp"
#include "func_rmdir_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Rmdir = FuncWrapper<Func2::RmdirBase,
                            Func2::RmdirFactory,
                            int,
                            const Branches&,
                            const fs::path&>;
}
