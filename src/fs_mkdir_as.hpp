#pragma once

#include "fs_mkdir.hpp"
#include "ugid.hpp"


#if defined __linux__
namespace fs
{
  template<typename T>
  static
  inline
  int
  mkdir_as(const ugid_t  ugid_,
           const T      &path_,
           const mode_t  mode_)
  {
    const ugid::SetGuard _(ugid_);

    return fs::mkdir(path_,mode_);
  }
}
#elif defined __FreeBSD__
#include "fs_lchown.hpp"

namespace fs
{
  template<typename T>
  static
  inline
  int
  mkdir_as(const ugid_t  ugid_,
           const T      &path_,
           const mode_t  mode_)
  {
    int rv;

    rv = fs::mkdir(path_,mode_);
    if(rv < 0)
      return rv;

    fs::lchown(path_,ugid_.uid,ugid_.gid);

    return 0;
  }
}
#else
#error "Not Supported"
#endif
