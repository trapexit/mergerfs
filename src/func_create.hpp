#pragma once

#include "func_create_base.hpp"
#include "func_create_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Create = FuncWrapper<Func2::CreateBase,
                             Func2::CreateFactory,
                             int,
                             const ugid_t&,
                             const Branches&,
                             const fs::path&,
                             fuse_file_info_t*&,
                             const mode_t&,
                             const mode_t&>;
}
