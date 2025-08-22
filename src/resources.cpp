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

#include "resources.hpp"

#include "fs_opendir.hpp"
#include "fs_closedir.hpp"
#include "fs_readdir.hpp"
#include "errno.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>


int
resources::reset_umask(void)
{
  umask(0);
  return 0;
}

int
resources::maxout_rlimit(const int resource)
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
    return -errno;

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
resources::maxout_rlimit_nofile(void)
{
  return maxout_rlimit(RLIMIT_NOFILE);
}

int
resources::maxout_rlimit_fsize(void)
{
  return maxout_rlimit(RLIMIT_FSIZE);
}

int
resources::setpriority(const int prio_)
{
  DIR *dir;
  struct dirent *e;

  ::setpriority(PRIO_PROCESS,0,prio_);

  dir = fs::opendir("/proc/self/task");
  if(dir == NULL)
    return -errno;

  while(true)
    {
      pid_t tid;

      e = fs::readdir(dir);
      if(e == NULL)
        break;
      if(e->d_name[0] == '.')
        continue;

      try
        {
          tid = std::stoi(e->d_name);
        }
      catch(...)
        {
          continue;
        }

      ::setpriority(PRIO_PROCESS,tid,prio_);
    }

  fs::closedir(dir);

  return 0;
}
