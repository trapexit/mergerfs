#pragma once

#include "func_statx_base.hpp"

namespace Func2
{
  class StatxFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<StatxBase> make(const std::string_view str);
  };
}
