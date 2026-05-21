#pragma once
#include "func_link_base.hpp"
namespace Func2 {
  class LinkEROFS : public LinkBase {
  public:
    LinkEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,const fs::path&);
  };
}
