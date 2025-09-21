#include "fs_open_fd.hpp"

#if defined(__linux__)
#include "fmt/core.h"
#include "fs_openat.hpp"
#include "procfs.hpp"

#include <cassert>

int
fs::open_fd(const int fd_,
            const int flags_)
{
  assert(procfs::PROC_SELF_FD_FD > 0);

  const int flags = (flags_ & ~O_NOFOLLOW);

  return fs::openat(procfs::PROC_SELF_FD_FD,
                    fmt::format("{}",fd_),
                    flags);
}
#elif defined(__FreeBSD__)
#include "fs_openat.hpp"

int
fs::open_fd(const int fd_,
            const int flags_)
{
  const int flags = ((flags_ | O_EMPTY_PATH) & ~O_NOFOLLOW);

  return fs::openat(fd_,"",flags);
}
#else
#error "fs::open_fd() not supported on platform"
#endif
