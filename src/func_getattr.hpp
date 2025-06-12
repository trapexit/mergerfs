#pragma once

#include "func_getattr_base.hpp"
#include "func_getattr_factory.hpp"

#include "tofrom_string.hpp"

#include "fmt/core.h"

#include <atomic>


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
    }

  public:
    int
    operator()(const Branches  &branches_,
               const fs::Path  &fusepath_,
               struct stat     *st_,
               fuse_timeouts_t *timeout_)
    {
      std::shared_ptr<Func2::GetAttrBase> p;

      p = std::atomic_load(&_impl);

      return (*p)(branches_,fusepath_,st_,timeout_);
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
    from_string(const std::string &str_)
    {
      std::shared_ptr<Func2::GetAttrBase> p;

      p = Func2::GetAttrFactory::make(str_);
      if(!p)
        return -EINVAL;

      _impl = std::atomic_load(&p);

      fmt::print("getattr: {}\n",
                 _impl->name());

      return 0;
    }
  };
}
