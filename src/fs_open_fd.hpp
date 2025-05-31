#pragma once


namespace fs
{
#ifdef __linux__
  static
  inline
  int
  open_fd(const int fd_,
          const int flags_)
  {
    return 0;
  }
#elif defined __FreeBSD__
  static
  inline
  int
  open_fd(const int fd_,
          const int flags_)
  {
    return 0;
  }
#else

#endif
}
