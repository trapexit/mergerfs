#pragma once

#include "func_chown_base.hpp"
#include "func_chown_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Chown = FuncWrapper<Func2::ChownBase,
                            Func2::ChownFactory,
                            int,
                            const Branches&,
                            const fs::path&,
                            const uid_t&,
                            const gid_t&>;
}
