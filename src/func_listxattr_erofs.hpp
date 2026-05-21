#pragma once
#include "func_listxattr_base.hpp"
namespace Func2 {
  class ListxattrEROFS : public ListxattrBase {
  public:
    ListxattrEROFS() {}
    std::string_view name() const;
    ssize_t operator()(const Branches&,const fs::path&,char*,const size_t);
  };
}
