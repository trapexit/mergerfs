#pragma once

#include "ugid.hpp"
#include "fs_symlink.hpp"
#include "fs_lchown.hpp"

#if defined __linux__
namespace fs
{
  template<typename T>
  static
  inline
  int
  symlink_as(const ugid_t  ugid_,
             const char   *target_,
             const T      &linkpath_)
  {
    const ugid::Set _(ugid_);

    return fs::symlink(target_,linkpath_);
  }
}
#elif defined __FreeBSD__
  template<typename T>
  static
  inline
  int
  symlink_as(const ugid_t  ugid_,
             const char   *target_,
             const T      &linkpath_)
  {
    int rv;

    rv = fs::symlink(target_,linkpath_);
    if(rv < 0)
      return rv;

    fs::lchown(linkpath_,ugid_.uid,ugid_.gid);

    return 0;
  }
}
#else
#error "Not Supported!"
#endif
