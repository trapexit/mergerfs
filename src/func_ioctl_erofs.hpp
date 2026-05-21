#pragma once
#include "func_ioctl_base.hpp"
namespace Func2 {
  class IoctlEROFS : public IoctlBase {
  public:
    IoctlEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,const uint32_t&,void*&,uint32_t*&);
  };
}
