#pragma once

#include "func_getattr_base.hpp"
#include "func_getattr_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using GetAttr = FuncWrapper<Func2::GetAttrBase,
                              Func2::GetAttrFactory,
                              int,
                              const Branches&,
                              const fs::path&,
                              struct stat*&,
                              const FollowSymlinksEnum&,
                              const s64&>;
}
