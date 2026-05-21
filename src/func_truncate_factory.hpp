#pragma once

#include "func_truncate_base.hpp"

namespace Func2
{
  class TruncateFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<TruncateBase> make(const std::string_view str);
  };
}
