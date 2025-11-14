#pragma once

#include "func_statx_base.hpp"
#include "func_statx_factory.hpp"

#include "func_wrapper.hpp"


namespace Func2
{
  using Statx = FuncWrapper<Func2::StatxBase,
                            Func2::StatxFactory,
                            int,
                            const Branches&,
                            const fs::path&,
                            const u32&,
                            const u32&,
                            struct fuse_statx*&,
                            const FollowSymlinksEnum,
                            const s64&>;
}
