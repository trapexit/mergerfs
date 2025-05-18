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
  return fs::openat(procfs::PROC_SELF_FD_FD,
                    fmt::format("{}",fd_),
                    flags_);
}
#elif defined(__FreeBSD__)
#include "fs_openat.hpp"

int
fs::open_fd(const int fd_,
            const int flags_)
{
  return fs::openat(fd_,"",flags_|O_EMPTY_PATH);
}
#else
int
fs::open_fd(const int fd_,
            const int flags_)
{
  return -ENOSUP;
}
#endif
