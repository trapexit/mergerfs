#pragma once

#include "func_getattr_base.hpp"
#include "func_getattr_combine.hpp"

#include "fmt/core.h"

#include <memory>

namespace Func2
{
  class GetAttr
  {
  public:
    GetAttr()
    {
      _impl = std::make_shared<Func2::GetAttrCombine>();
    }

  public:
    int
    operator()(const Branches  &branches_,
               const fs::Path  &fusepath_,
               struct stat     *st_,
               fuse_timeouts_t *timeout_)
    {
      std::shared_ptr<Func2::GetAttrBase> p;

      {
        std::lock_guard<std::mutex> guard{_lock};
        p = _impl;
      }

      return (*p)(branches_,fusepath_,st_,timeout_);
    }

  private:
    std::mutex _lock;
    std::shared_ptr<Func2::GetAttrBase> _impl;
  };
}
