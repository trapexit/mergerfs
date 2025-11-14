#pragma once

#include "func_removexattr_base.hpp"
#include "func_removexattr_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Removexattr = FuncWrapper<Func2::RemovexattrBase,
                                  Func2::RemovexattrFactory,
                                  int,
                                  const Branches&,
                                  const fs::path&,
                                  const char*&>;
}
