#include "ioprio.hpp"

#ifdef __linux__
# include <linux/ioprio.h>
# include <sys/syscall.h>
# include <unistd.h>
# include <errno.h>
#else
#warning "ioprio not supported on this platform"
#endif

thread_local int ioprio::SetFrom::thread_prio = 0;

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

ioprio::SetFrom::SetFrom(const pid_t pid_)
{
  int client_prio;

  client_prio = ioprio::get(IOPRIO_WHO_PROCESS,pid_);
  if(client_prio < 0)
    return;
  if(client_prio == thread_prio)
    return;

  ioprio::set(IOPRIO_WHO_PROCESS,0,client_prio);
}
