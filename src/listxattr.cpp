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

#include <sys/types.h>
#include <attr/xattr.h>
#include <errno.h>

#include "config.hpp"
#include "fs.hpp"
#include "ugid.hpp"
#include "assert.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;

static
int
_listxattr(const Policy::Search::Func  searchFunc,
           const vector<string>       &srcmounts,
           const string                fusepath,
           char                       *list,
           const size_t                size)
{
#ifndef WITHOUT_XATTR
  int rv;
  string path;

  path = searchFunc(srcmounts,fusepath).full;
  if(path.empty())
    return -ENOENT;

  rv = ::llistxattr(path.c_str(),list,size);

  return ((rv == -1) ? -errno : rv);
#else
  return -ENOTSUP;
#endif
}

namespace mergerfs
{
  namespace listxattr
  {
    int
    listxattr(const char *fusepath,
              char       *list,
              size_t      size)
    {
      const ugid::SetResetGuard  ugid;
      const config::Config      &config = config::get();

      if(fusepath == config.controlfile)
        return -ENOTSUP;

      return _listxattr(config.policy.search,
                        config.srcmounts,
                        fusepath,
                        list,
                        size);
    }
  }
}
