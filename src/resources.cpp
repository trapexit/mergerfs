/*
   The MIT License (MIT)

   Copyright (c) 2014 Antonio SJ Musumeci <trapexit@spawn.link>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

namespace mergerfs
{
  namespace resources
  {
    int
    reset_umask(void)
    {
      umask(0);
      return 0;
    }

    int
    maxout_rlimit(int resource)
    {
      struct rlimit rlim;

      if(geteuid() == 0)
        {
          rlim.rlim_cur = RLIM_INFINITY;
          rlim.rlim_max = RLIM_INFINITY;
        }
      else
        {
          int rv;

          rv = ::getrlimit(resource,&rlim);
          if(rv == -1)
            return -1;

          rlim.rlim_cur = rlim.rlim_max;
        }

      return ::setrlimit(resource,&rlim);
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
  }
}
