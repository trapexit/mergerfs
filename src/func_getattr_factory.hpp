#pragma once

#include "func_getattr_base.hpp"

namespace Func2
{
  class GetAttrFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<GetAttrBase> make(const std::string str);
  };
}
