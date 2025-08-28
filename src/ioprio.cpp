#include "ioprio.hpp"

#ifdef __linux__
# include <linux/ioprio.h>
# include <sys/syscall.h>
# include <unistd.h>
#else
#warning "ioprio not supported on this platform"
#endif

int
ioprio::get(const int which_,
            const int who_)
{
  int rv;

  rv = syscall(SYS_ioprio_get,which_,who_);

  return ((rv == -1) ? -errno : rv);
}

int
ioprio::set(const int which_,
            const int who_,
            const int ioprio_)
{
  int rv;

  rv = syscall(SYS_ioprio_set,which_,who_,ioprio_);

  return ((rv == -1) ? -errno : rv);
}
