#pragma once

#include "branches.hpp"
#include "fs_path.hpp"
#include "ugid.hpp"

#include "fuse.h"


namespace Func2
{
  class CreateBase
  {
  public:
    CreateBase() {}
    virtual ~CreateBase() = default;

  public:
    virtual std::string_view name() const = 0;
    virtual bool path_preserving() const = 0;

  public:
    virtual int operator()(const ugid_t      &ugid,
                           const Branches    &branches,
                           const fs::path    &fusepath,
                           fuse_file_info_t *&ffi,
                           const mode_t      &mode,
                           const mode_t      &umask) = 0;
  };
}
