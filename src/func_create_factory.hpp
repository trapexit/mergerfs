#pragma once

#include <memory>
#include <string_view>

namespace Func2
{
  class CreateBase;

  class CreateFactory
  {
  public:
    static bool valid(const std::string str_);

    static std::shared_ptr<CreateBase> make(const std::string_view str_);
  };
}
