#pragma once

#include "func_chmod_base.hpp"

namespace Func2
{
  class ChmodFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<ChmodBase> make(const std::string_view str);
  };
}
