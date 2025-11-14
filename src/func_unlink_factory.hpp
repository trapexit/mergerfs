#pragma once

#include "func_unlink_base.hpp"

namespace Func2
{
  class UnlinkFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<UnlinkBase> make(const std::string_view str);
  };
}
