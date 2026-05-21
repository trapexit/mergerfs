#pragma once

#include "func_create_base.hpp"

namespace Func2
{
  class CreateMSPMFS : public CreateBase
  {
  public:
    CreateMSPMFS() {}
    ~CreateMSPMFS() {}
  public:
    std::string_view name() const;
    bool path_preserving() const;
  public:
    int operator()(const ugid_t      &ugid,
                   const Branches    &branches,
                   const fs::path    &fusepath,
                   fuse_file_info_t *&ffi,
                   const mode_t      &mode,
                   const mode_t      &umask);
  };
}
