#pragma once

#include "func_mkdir_base.hpp"
#include "func_mkdir_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Mkdir = FuncWrapper<Func2::MkdirBase,
                            Func2::MkdirFactory,
                            int,
                            const ugid_t&,
                            const Branches&,
                            const fs::path&,
                            const mode_t&,
                            const mode_t&>;
}
