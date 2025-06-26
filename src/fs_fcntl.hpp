#pragma once

#include <fcntl.h>

namespace fs
{
  static
  inline
  int
  fcntl(const int fd_,
        const int op_)
  {
    int rv;

    rv = ::fcntl(fd_,op_);

    return ((rv == -1) ? -errno : rv);
  }

  static
  inline
  int
  fcntl(const int fd_,
        const int op_,
        const int arg_)
  {
    int rv;

    rv = ::fcntl(fd_,op_,arg_);

    return ((rv == -1) ? -errno : rv);
  }

  static
  inline
  int
  fcntl_setlease_rdlck(const int fd_)
  {
#if defined(F_SETLEASE) && defined(F_RDLCK)
    return fs::fcntl(fd_,F_SETLEASE,F_RDLCK);
#else
    return -ENOTSUP;
#endif
  }

  static
  inline
  int
  fcntl_setlease_unlck(const int fd_)
  {
#if defined(F_SETLEASE) && defined(F_UNLCK)
    return fs::fcntl(fd_,F_SETLEASE,F_UNLCK);
#else
    return -ENOTSUP;
#endif

  }
}
