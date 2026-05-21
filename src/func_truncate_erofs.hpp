#pragma once
#include "func_truncate_base.hpp"
namespace Func2 {
  class TruncateEROFS : public TruncateBase {
  public:
    TruncateEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,const off_t);
  };
}
