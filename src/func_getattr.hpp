#pragma once

#include "func_getattr_base.hpp"
#include "func_getattr_factory.hpp"

#include "tofrom_string.hpp"

#include "fmt/core.h"

#include <atomic>

#include <cassert>

namespace Func2
{
  class GetAttr : public ToFromString
  {
  private:
    std::shared_ptr<Func2::GetAttrBase> _impl;

  public:
    GetAttr()
    {
    }

    GetAttr(const std::string &name_)
    {
      _impl = Func2::GetAttrFactory::make(name_);
      assert(_impl);
    }

  public:
    int
    operator()(const Branches  &branches_,
               const fs::path  &fusepath_,
               struct stat     *st_,
               const FollowSymlinksEnum follow_symlinks_,
               const s64 symlinkify_timeout_)
    {
      std::shared_ptr<Func2::GetAttrBase> p;

      p = std::atomic_load(&_impl);

      return (*p)(branches_,
                  fusepath_,
                  st_,
                  follow_symlinks_,
                  symlinkify_timeout_);
    }

  public:
    std::string
    to_string() const
    {
      std::shared_ptr<Func2::GetAttrBase> p;

      p = std::atomic_load(&_impl);

      return std::string(p->name());
    }

    int
    from_string(const std::string_view str_)
    {
      std::shared_ptr<Func2::GetAttrBase> p;

      p = Func2::GetAttrFactory::make(str_);
      if(!p)
        return -EINVAL;

      _impl = std::atomic_load(&p);

      return 0;
    }
  };
}
