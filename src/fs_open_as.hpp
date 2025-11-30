#pragma once

#include "ugid.hpp"
#include "fs_open.hpp"

#if defined __linux__
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
#elif defined __FreeBSD__
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
    int rv;

    rv = fs::open(path_,flags_,mode_);
    if(rv < 0)
      return rv;

    fs::fchown(rv,ugid_.uid,ugid_.gid);

    return rv;
  }
}
#else
#error "Not Supported!"
#endif
