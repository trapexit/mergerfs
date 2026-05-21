#pragma once
#include "func_readlink_base.hpp"
namespace Func2 {
  class ReadlinkEROFS : public ReadlinkBase {
  public:
    ReadlinkEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,char*,const size_t,const bool,const time_t);
  };
}
