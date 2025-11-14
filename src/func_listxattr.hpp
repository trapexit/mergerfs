#pragma once

#include "func_listxattr_base.hpp"
#include "func_listxattr_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Listxattr = FuncWrapper<Func2::ListxattrBase,
                                Func2::ListxattrFactory,
                                int,
                                const Branches&,
                                const fs::path&,
                                const mode_t&>;
}
