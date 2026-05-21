#pragma once

#include "func_readlink_base.hpp"

namespace Func2
{
  class ReadlinkFactory
  {
  public:
    static bool valid(const std::string str);
    static std::shared_ptr<ReadlinkBase> make(const std::string_view str);
  };
}
