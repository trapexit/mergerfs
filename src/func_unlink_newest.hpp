#pragma once
#include "func_unlink_base.hpp"
namespace Func2 {
  class UnlinkNewest : public UnlinkBase {
  public:
    UnlinkNewest() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&);
  };
}
