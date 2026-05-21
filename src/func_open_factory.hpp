#pragma once

#include "func_open_base.hpp"

namespace Func2
{
  class OpenFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<OpenBase> make(const std::string_view str);
  };
}
