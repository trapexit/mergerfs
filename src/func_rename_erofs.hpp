#pragma once
#include "func_rename_base.hpp"
namespace Func2 {
  class RenameEROFS : public RenameBase {
  public:
    RenameEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,const fs::path&);
  };
}
