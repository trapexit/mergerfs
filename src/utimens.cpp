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

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

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
_utimens(const fs::find::Func   actionFunc,
         const vector<string>  &srcmounts,
         const string          &fusepath,
         const struct timespec  ts[2])
{
  int rv;
  int error;
  fs::Paths paths;

  rv = actionFunc(srcmounts,fusepath,paths,-1);
  if(rv == -1)
    return -errno;

  error = 0;
  for(fs::Paths::const_iterator
        i = paths.begin(), ei = paths.end(); i != ei; ++i)
    {
      rv = ::utimensat(0,i->full.c_str(),ts,AT_SYMLINK_NOFOLLOW);
      if(rv == -1)
        error = errno;
    }

  return -error;
}

namespace mergerfs
{
  namespace utimens
  {
    int
    utimens(const char            *fusepath,
            const struct timespec  ts[2])
    {
      const struct fuse_context *fc     = fuse_get_context();
      const config::Config      &config = config::get();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard    readlock(&config.srcmountslock);

      return _utimens(*config.utimens,
                      config.srcmounts,
                      fusepath,
                      ts);
    }
  }
}
