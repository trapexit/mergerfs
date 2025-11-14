#pragma once

#include "func_chmod_base.hpp"
#include "func_chmod_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Chmod = FuncWrapper<Func2::ChmodBase,
                            Func2::ChmodFactory,
                            int,
                            const Branches&,
                            const fs::path&,
                            const mode_t&>;
}
