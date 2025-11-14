#pragma once

#include "func_setxattr_base.hpp"

namespace Func2
{
  class SetxattrFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<SetxattrBase> make(const std::string_view str);
  };
}
