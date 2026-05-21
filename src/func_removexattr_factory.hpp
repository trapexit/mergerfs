#pragma once

#include "func_removexattr_base.hpp"

namespace Func2
{
  class RemovexattrFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<RemovexattrBase> make(const std::string_view str);
  };
}
