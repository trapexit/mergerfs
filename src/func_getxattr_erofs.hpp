#pragma once
#include "func_getxattr_base.hpp"
namespace Func2 {
  class GetxattrEROFS : public GetxattrBase {
  public:
    GetxattrEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,const char*,char*,const size_t);
  };
}
