#pragma once

#include "fs_mknod.hpp"
#include "ugid.hpp"


#if defined __linux__
namespace fs
{
  template<typename T>
  static
  inline
  int
  mknod_as(const ugid_t  ugid_,
           const T      &path_,
           const mode_t  mode_,
           const dev_t   dev_)
  {
    const ugid::SetGuard _(ugid_);

    return fs::mknod(path_,mode_,dev_);
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
  mknod_as(const ugid_t  ugid_,
           const T      &path_,
           const mode_t  mode_,
           const dev_t   dev_)
  {
    int rv;

    rv = fs::mknod(path_,mode_,dev_);

    fs::lchown(path_,ugid_.uid,ugid_.gid);

    return rv;
  }
}
#else
#error "Not Supported!"
#endif
