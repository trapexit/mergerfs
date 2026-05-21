#pragma once

#include <memory>
#include <string_view>

namespace Func2
{
  class MkdirBase;

  class MkdirFactory
  {
  public:
    static bool valid(const std::string str_);

    static std::shared_ptr<MkdirBase> make(const std::string_view str_);
  };
}
