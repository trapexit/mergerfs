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

#include <fuse.h>

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include <string>
#include <vector>

#include "ugid.hpp"
#include "fs.hpp"
#include "config.hpp"
#include "rwlock.hpp"

using std::string;
using std::vector;

static
int
_truncate(const fs::find::Func  actionFunc,
          const vector<string> &srcmounts,
          const size_t          minfreespace,
          const string         &fusepath,
          const off_t           size)
{
  int rv;
  int error;
  Paths paths;

  rv = actionFunc(srcmounts,fusepath,minfreespace,paths);
  if(rv == -1)
    return -errno;

  error = 0;
  for(Paths::const_iterator
        i = paths.begin(), ei = paths.end(); i != ei; ++i)
    {
      rv = ::truncate(i->full.c_str(),size);
      if(rv == -1)
        error = errno;
    }

  return -error;
}

namespace mergerfs
{
  namespace truncate
  {
    int
    truncate(const char *fusepath,
             off_t       size)
    {
      const struct fuse_context *fc     = fuse_get_context();
      const config::Config      &config = config::get();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard    readlock(&config.srcmountslock);

      return _truncate(*config.truncate,
                       config.srcmounts,
                       config.minfreespace,
                       fusepath,
                       size);
    }
  }
}
