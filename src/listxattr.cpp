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

#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "config.hpp"
#include "category.hpp"
#include "fs.hpp"
#include "ugid.hpp"
#include "assert.hpp"
#include "xattr.hpp"

using std::string;
using std::vector;
using namespace mergerfs;

static
int
_listxattr_controlfile(char         *list,
                       const size_t  size)
{
  size_t xattrssize;
  string xattrs;

  for(int i = Category::Enum::BEGIN; i < Category::Enum::END; ++i)
    xattrs += "user.mergerfs." + (string)Category::categories[i] + '\0';

  xattrssize = xattrs.size();

  if(size == 0)
    return xattrssize;

  if(size < xattrssize)
    return -ERANGE;

  memcpy(list,xattrs.data(),xattrssize);

  return xattrssize;
}

static
int
_listxattr(const fs::SearchFunc  searchFunc,
           const vector<string> &srcmounts,
           const string          fusepath,
           char                 *list,
           const size_t          size)
{
#ifndef WITHOUT_XATTR
  int rv;
  fs::PathVector paths;

  searchFunc(srcmounts,fusepath,paths);
  if(paths.empty())
    return -ENOENT;

  rv = ::llistxattr(paths[0].full.c_str(),list,size);

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
      const struct fuse_context *fc = fuse_get_context();
      const config::Config      &config = config::get();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);

      if(fusepath == config.controlfile)
        return _listxattr_controlfile(list,
                                      size);

      return _listxattr(*config.search,
                        config.srcmounts,
                        fusepath,
                        list,
                        size);
    }
  }
}
