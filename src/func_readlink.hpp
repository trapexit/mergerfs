#pragma once

#include "func_readlink_base.hpp"
#include "func_readlink_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Readlink = FuncWrapper<Func2::ReadlinkBase,
                               Func2::ReadlinkFactory,
                               int,
                               const Branches&,
                               const fs::path&,
                               char *&,
                               const size_t&,
                               const time_t&>;
}
