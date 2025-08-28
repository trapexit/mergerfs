#include "ioprio.hpp"

#ifdef __linux__
# include <linux/ioprio.h>
# include <sys/syscall.h>
# include <unistd.h>
# include <errno.h>
#else
#warning "ioprio not supported on this platform"
#endif

thread_local int ioprio::SetFrom::thread_prio = -1;
bool _enabled = false;

int
ioprio::get(const int which_,
            const int who_)
{
#ifdef SYS_ioprio_get
  int rv;

  rv = syscall(SYS_ioprio_get,which_,who_);

  return ((rv == -1) ? -errno : rv);
#else
  return -ENOSUP;
#endif
}

int
ioprio::set(const int which_,
            const int who_,
            const int ioprio_)
{
#ifdef SYS_ioprio_set
  int rv;

  rv = syscall(SYS_ioprio_set,which_,who_,ioprio_);

  return ((rv == -1) ? -errno : rv);
#else
  return -ENOSUP;
#endif
}

void
ioprio::enable(const bool enable_)
{
  _enabled = enable_;
}

bool
ioprio::enabled()
{
  return _enabled;
}

ioprio::SetFrom::SetFrom(const pid_t pid_)
{
  int client_prio;

  if(!_enabled)
    return;

  client_prio = ioprio::get(IOPRIO_WHO_PROCESS,pid_);
  if(client_prio < 0)
    return;
  if(client_prio == thread_prio)
    return;

  thread_prio = client_prio;
  ioprio::set(IOPRIO_WHO_PROCESS,0,client_prio);
}
