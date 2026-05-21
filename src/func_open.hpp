#pragma once

#include "func_open_base.hpp"
#include "func_open_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Open = FuncWrapper<Func2::OpenBase,
                           Func2::OpenFactory,
                           int,
                           const Branches&,
                           const fs::path&,
                           fuse_file_info_t*&,
                           const bool&,
                           const NFSOpenHack&>;
}
