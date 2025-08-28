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
  return 0;
}

int
ioprio::set(const int which_,
            const int who_,
            const int ioprio_)
{
  return 0;
}
