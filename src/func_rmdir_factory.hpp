#pragma once

#include "func_rmdir_base.hpp"

namespace Func2
{
  class RmdirFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<RmdirBase> make(const std::string_view str);
  };
}
