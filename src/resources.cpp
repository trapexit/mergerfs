/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

namespace resources
{
  int
  reset_umask(void)
  {
    umask(0);
    return 0;
  }

  int
  maxout_rlimit(const int resource)
  {
    int rv;
    struct rlimit rlim;

    rlim.rlim_cur = RLIM_INFINITY;
    rlim.rlim_max = RLIM_INFINITY;
    rv = ::setrlimit(resource,&rlim);
    if(rv == 0)
      return 0;

    rv = ::getrlimit(resource,&rlim);
    if(rv == -1)
      return -1;

    rv = 0;
    rlim.rlim_cur = rlim.rlim_max;
    while(rv == 0)
      {
        rv = ::setrlimit(resource,&rlim);
        rlim.rlim_max *= 2;
        rlim.rlim_cur  = rlim.rlim_max;
      }

    return rv;
  }

  int
  maxout_rlimit_nofile(void)
  {
    return maxout_rlimit(RLIMIT_NOFILE);
  }

  int
  maxout_rlimit_fsize(void)
  {
    return maxout_rlimit(RLIMIT_FSIZE);
  }

  int
  setpriority(const int prio)
  {
    const int SELF = 0;

    return ::setpriority(PRIO_PROCESS,SELF,prio);
  }
}
