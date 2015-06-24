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

#include <string>
#include <vector>

#include <errno.h>
#include <sys/types.h>

#include "config.hpp"
#include "ugid.hpp"
#include "fs.hpp"
#include "rwlock.hpp"
#include "xattr.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;

static
int
_removexattr(const Policy::Func::Ptr  actionFunc,
             const vector<string>    &srcmounts,
             const size_t             minfreespace,
             const string            &fusepath,
             const char              *attrname)
{
#ifndef WITHOUT_XATTR
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
      rv = ::lremovexattr(i->full.c_str(),attrname);
      if(rv == -1)
        error = errno;
    }

  return -error;
#else
  return -ENOTSUP;
#endif
}

namespace mergerfs
{
  namespace removexattr
  {
    int
    removexattr(const char *fusepath,
                const char *attrname)
    {
      const config::Config &config = config::get();

      if(fusepath == config.controlfile)
        return -ENOTSUP;

      const struct fuse_context *fc = fuse_get_context();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard    readlock(&config.srcmountslock);

      return _removexattr(*config.removexattr,
                          config.srcmounts,
                          config.minfreespace,
                          fusepath,
                          attrname);
    }
  }
}
