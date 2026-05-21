#pragma once

#include "func_getxattr_base.hpp"
#include "func_getxattr_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Getxattr = FuncWrapper<Func2::GetxattrBase,
                               Func2::GetxattrFactory,
                               int,
                               const Branches&,
                               const fs::path&,
                               const char*&,
                               char*&,
                               const size_t&>;
}
