#pragma once

#include "func_ioctl_base.hpp"
#include "func_ioctl_factory.hpp"

#include "func_wrapper.hpp"

namespace Func2
{
  using Ioctl = FuncWrapper<Func2::IoctlBase,
                            Func2::IoctlFactory,
                            int,
                            const Branches&,
                            const fs::path&,
                            const mode_t&>;
}
