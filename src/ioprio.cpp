/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "ioprio.hpp"

#include <errno.h>

#ifdef __linux__
# include <sys/syscall.h>
# include <unistd.h>
#else
#pragma message "ioprio not supported on this platform"
#endif

enum
  {
    IOPRIO_WHO_PROCESS = 1
  };

namespace ioprio
{
  std::atomic<bool> _enabled{false};
}

thread_local int ioprio::SetFrom::thread_prio = -1;

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
  _enabled.store(enable_,std::memory_order_relaxed);
}

void
ioprio::SetFrom::_slow_apply(const pid_t pid_)
{
  int client_prio;

  client_prio = ioprio::get(pid_);
  if(client_prio < 0)
    return;
  if(client_prio == thread_prio)
    return;

  thread_prio = client_prio;
  ioprio::set(0,client_prio);
}
