#pragma once
#include "func_removexattr_base.hpp"
namespace Func2 {
  class RemovexattrEROFS : public RemovexattrBase {
  public:
    RemovexattrEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,const char*);
  };
}
