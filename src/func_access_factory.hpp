#pragma once

#include "func_access_base.hpp"

namespace Func2
{
  class AccessFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<AccessBase> make(const std::string_view str);
  };
}
