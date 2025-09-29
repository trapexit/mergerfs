#include "ioprio.hpp"

#include <errno.h>

#ifdef __linux__
# include <sys/syscall.h>
# include <unistd.h>
#else
#warning "ioprio not supported on this platform"
#endif

enum
  {
    IOPRIO_WHO_PROCESS = 1
  };

thread_local int ioprio::SetFrom::thread_prio = -1;
bool _enabled = false;

int
ioprio::get(const int who_)
{
#ifdef SYS_ioprio_get
  int rv;
  const int which = IOPRIO_WHO_PROCESS;

  rv = syscall(SYS_ioprio_get,which,who_);

  return ((rv == -1) ? -errno : rv);
#else
  return -EOPNOTSUPP;
#endif
}

int
ioprio::set(const int who_,
            const int ioprio_)
{
#ifdef SYS_ioprio_set
  int rv;
  const int which = IOPRIO_WHO_PROCESS;

  rv = syscall(SYS_ioprio_set,which,who_,ioprio_);

  return ((rv == -1) ? -errno : rv);
#else
  return -EOPNOTSUPP;
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

  client_prio = ioprio::get(pid_);
  if(client_prio < 0)
    return;
  if(client_prio == thread_prio)
    return;

  thread_prio = client_prio;
  ioprio::set(0,client_prio);
}
