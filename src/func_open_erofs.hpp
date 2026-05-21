#pragma once
#include "func_open_base.hpp"
namespace Func2 {
  class OpenEROFS : public OpenBase {
  public:
    OpenEROFS() {}
    std::string_view name() const;
    int operator()(const Branches&,const fs::path&,fuse_file_info_t*&,const bool&,const NFSOpenHack&);
  };
}
