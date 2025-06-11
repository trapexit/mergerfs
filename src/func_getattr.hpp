#pragma once

#include "func_getattr_base.hpp"

#include "tofrom_string.hpp"

#include "fmt/core.h"

#include <atomic>


namespace Func2
{
  class GetAttr : public ToFromString
  {
  public:
    GetAttr()
    {
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

  private:
    std::shared_ptr<Func2::GetAttrBase> _impl;
  };
}
