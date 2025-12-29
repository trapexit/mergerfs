#pragma once

#include <memory>
#include <string_view>

namespace Func2
{
  class MknodBase;

  class MknodFactory
  {
  public:
    static bool valid(const std::string str_);

    static std::shared_ptr<MknodBase> make(const std::string_view str_);
  };
}