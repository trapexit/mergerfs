#pragma once

#include <memory>
#include <string_view>

namespace Func2
{
  class SymlinkBase;

  class SymlinkFactory
  {
  public:
    static bool valid(const std::string str_);

    static std::shared_ptr<SymlinkBase> make(const std::string_view str_);
  };
}
