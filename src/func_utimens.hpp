#pragma once

#include "func_utimens_base.hpp"
#include "func_utimens_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Utimens = FuncWrapper<Func2::UtimensBase,
                              Func2::UtimensFactory,
                              int,
                              const Branches&,
                              const fs::path&,
                              const timespec*&>;
}
