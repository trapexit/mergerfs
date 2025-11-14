#pragma once

#include "func_chown_base.hpp"

namespace Func2
{
  class ChownFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<ChownBase> make(const std::string_view str);
  };
}
