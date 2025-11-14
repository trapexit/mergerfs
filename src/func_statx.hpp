#pragma once

#include "func_statx_base.hpp"
#include "func_statx_factory.hpp"

#include "tofrom_string.hpp"

#include "fmt/core.h"

#include <atomic>

#include <assert.h>


namespace Func2
{
  class Statx : public ToFromString
  {
  private:
    std::shared_ptr<Func2::StatxBase> _impl;

  public:
    Statx()
    {
    }

    Statx(const std::string &name_)
    {
      _impl = Func2::StatxFactory::make(name_);
      assert(_impl);
    }

  public:
    int
    operator()(const Branches           &branches_,
               const fs::path           &fusepath_,
               const u32                 flags_,
               const u32                 mask_,
               struct fuse_statx        *st_,
               const FollowSymlinksEnum  follow_symlinks_,
               const s64                 symlinkify_timeout_)
    {
      std::shared_ptr<Func2::StatxBase> p;

      p = std::atomic_load(&_impl);

      return (*p)(branches_,
                  fusepath_,
                  flags_,
                  mask_,
                  st_,
                  follow_symlinks_,
                  symlinkify_timeout_);
    }

  public:
    std::string
    to_string() const
    {
      std::shared_ptr<Func2::StatxBase> p;

      p = std::atomic_load(&_impl);

      return std::string(p->name());
    }

    int
    from_string(const std::string_view str_)
    {
      std::shared_ptr<Func2::StatxBase> p;

      p = Func2::StatxFactory::make(str_);
      if(!p)
        return -EINVAL;

      _impl = std::atomic_load(&p);

      return 0;
    }
  };
}
