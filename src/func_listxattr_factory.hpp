#pragma once

#include "func_listxattr_base.hpp"

namespace Func2
{
  class ListxattrFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<ListxattrBase> make(const std::string_view str);
  };
}
