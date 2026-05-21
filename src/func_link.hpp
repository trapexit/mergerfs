#pragma once

#include "func_link_base.hpp"
#include "func_link_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Link = FuncWrapper<Func2::LinkBase,
                           Func2::LinkFactory,
                            int,
                            const Branches&,
                            const fs::path&,
                            const fs::path&>;
}
