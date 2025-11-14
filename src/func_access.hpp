#pragma once

#include "func_access_base.hpp"
#include "func_access_factory.hpp"

#include "func_wrapper.hpp"

namespace Func2
{
  using Access = FuncWrapper<Func2::AccessBase,
                             Func2::AccessFactory,
                             int,
                             const Branches&,
                             const fs::path&,
                             const int&>;
}
