#pragma once

#include "func_truncate_base.hpp"
#include "func_truncate_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Truncate = FuncWrapper<Func2::TruncateBase,
                               Func2::TruncateFactory,
                               int,
                               const Branches&,
                               const fs::path&,
                               const off_t&>;
}
