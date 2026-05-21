#pragma once
#include "func_chown_base.hpp"
namespace Func2 {
  class ChownEROFS : public ChownBase {
  public:
    ChownEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,const uid_t,const gid_t);
  };
}
