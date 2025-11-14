#pragma once

#include "func_setxattr_base.hpp"
#include "func_setxattr_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Setxattr = FuncWrapper<Func2::SetxattrBase,
                               Func2::SetxattrFactory,
                               int,
                               const Branches&,
                               const fs::path&,
                               const char*&,
                               const char*&,
                               const size_t&,
                               const int&>;
}
