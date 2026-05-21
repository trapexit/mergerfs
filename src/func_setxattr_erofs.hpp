#pragma once
#include "func_setxattr_base.hpp"
namespace Func2 {
  class SetxattrEROFS : public SetxattrBase {
  public:
    SetxattrEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,const char*,const char*,size_t,int);
  };
}
