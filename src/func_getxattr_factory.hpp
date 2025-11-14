#pragma once

#include "func_getxattr_base.hpp"

namespace Func2
{
  class GetxattrFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<GetxattrBase> make(const std::string_view str);
  };
}
