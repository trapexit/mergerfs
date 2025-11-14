#pragma once

#include "func_ioctl_base.hpp"

namespace Func2
{
  class IoctlFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<IoctlBase> make(const std::string_view str);
  };
}
