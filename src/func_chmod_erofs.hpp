#pragma once
#include "func_chmod_base.hpp"
namespace Func2 {
  class ChmodEROFS : public ChmodBase {
  public:
    ChmodEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,const mode_t);
  };
}
