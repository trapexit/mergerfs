#pragma once

#include "func_open_base.hpp"

namespace Func2
{
  class OpenFF : public OpenBase
  {
  public:
    OpenFF() {}
    ~OpenFF() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath,
                   fuse_file_info_t *&ffi,
                   const bool       &link_cow,
                   const NFSOpenHack &nfsopenhack);
  };
}
