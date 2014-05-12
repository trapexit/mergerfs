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

#include <string>
#include <vector>

#include <errno.h>
#include <sys/types.h>
#include <attr/xattr.h>

#include "config.hpp"
#include "fs.hpp"
#include "ugid.hpp"
#include "assert.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;

static
int
_setxattr(const Policy::Action::Func  searchFunc,
          const vector<string>       &srcmounts,
          const string                fusepath,
          const char                 *attrname,
          const char                 *attrval,
          const size_t                attrvalsize,
          const int                   flags)
{
#ifndef WITHOUT_XATTR
  int rv;
  int error;
  vector<fs::Path> paths;

  searchFunc(srcmounts,fusepath,paths);
  if(paths.empty())
    return -ENOENT;

  rv    = -1;
  error =  0;
  for(vector<fs::Path>::const_iterator
        i = paths.begin(), ei = paths.end(); i != ei; ++i)
    {
      rv &= ::lsetxattr(i->full.c_str(),attrname,attrval,attrvalsize,flags);
      if(rv == -1)
        error = errno;
    }

  return ((rv == -1) ? -error : 0);
#else
  return -ENOTSUP;
#endif
}
namespace mergerfs
{
  namespace setxattr
  {
    int
    setxattr(const char *fusepath,
             const char *attrname,
             const char *attrval,
             size_t      attrvalsize,
             int         flags)
    {
      const ugid::SetResetGuard  uid;
      const config::Config      &config = config::get();

      if(fusepath == config.controlfile)
        return -ENOTSUP;

      return _setxattr(config.policy.action,
                       config.srcmounts,
                       fusepath,
                       attrname,
                       attrval,
                       attrvalsize,
                       flags);
    }
  }
}
