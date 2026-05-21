#pragma once
#include "func_rmdir_base.hpp"
namespace Func2 {
  class RmdirEROFS : public RmdirBase {
  public:
    RmdirEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&);
  };
}
