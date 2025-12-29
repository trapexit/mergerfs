#pragma once

#include "func_mknod_base.hpp"
#include "func_mknod_factory.hpp"

#include "func_wrapper.hpp"

namespace Func2
{
  using Mknod = FuncWrapper<Func2::MknodBase,
                            Func2::MknodFactory,
                            int,
                            const ugid_t&,
                            const Branches&,
                            const fs::path&,
                            const mode_t&,
                            const mode_t&,
                            const dev_t&>;
}