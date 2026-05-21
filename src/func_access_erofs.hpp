#pragma once
#include "func_access_base.hpp"
namespace Func2 {
  class AccessEROFS : public AccessBase {
  public:
    AccessEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,const int);
  };
}
