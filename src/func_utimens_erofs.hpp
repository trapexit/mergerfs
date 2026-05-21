#pragma once
#include "func_utimens_base.hpp"
namespace Func2 {
  class UtimensEROFS : public UtimensBase {
  public:
    UtimensEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,const timespec[2]);
  };
}
