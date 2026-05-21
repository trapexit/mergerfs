#pragma once

#include "func_utimens_base.hpp"

namespace Func2
{
  class UtimensFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<UtimensBase> make(const std::string_view str);
  };
}
