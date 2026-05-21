#pragma once

#include <memory>
#include <string_view>

namespace Func2
{
  class LinkBase;

  class LinkFactory
  {
  public:
    static bool valid(const std::string str_);

    static std::shared_ptr<LinkBase> make(const std::string_view str_);
  };
}
