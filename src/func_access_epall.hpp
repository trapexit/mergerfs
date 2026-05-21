#pragma once
#include "func_access_base.hpp"
namespace Func2 {
  class AccessEPAll : public AccessBase {
  public:
    AccessEPAll() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,const int);
  };
}
