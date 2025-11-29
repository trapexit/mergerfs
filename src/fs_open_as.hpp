#pragma once

#include "ugid.hpp"
#include "fs_open.hpp"

namespace fs
{
  static
  inline
  int
  open_as(const ugid_t    ugid_,
          const fs::path &path_,
          const int       flags_,
          const mode_t    mode_)
  {
    const ugid::Set _(ugid_);

    return fs::open(path_,flags_,mode_);
  }
}
