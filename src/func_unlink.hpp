#pragma once

#include "func_unlink_base.hpp"
#include "func_unlink_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Unlink = FuncWrapper<Func2::UnlinkBase,
                             Func2::UnlinkFactory,
                             int,
                             const Branches&,
                             const fs::path&>;
}
