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
#include "rwlock.hpp"
#include "xattr.hpp"
#include "buildvector.hpp"

using std::string;
using std::vector;
using namespace mergerfs;

static
int
_listxattr_controlfile(char         *list,
                       const size_t  size)
{
  string xattrs;
  const vector<string> strs =
    buildvector<string>
    ("user.mergerfs.srcmounts")
    ("user.mergerfs.minfreespace");

  xattrs.reserve(512);
  for(size_t i = 0; i < strs.size(); i++)
    xattrs += (strs[i] + '\0');
  for(size_t i = Category::Enum::BEGIN; i < Category::Enum::END; i++)
    xattrs += ("user.mergerfs.category." + (std::string)*Category::categories[i] + '\0');
  for(size_t i = FuseFunc::Enum::BEGIN; i < FuseFunc::Enum::END; i++)
    xattrs += ("user.mergerfs.func." + (std::string)*FuseFunc::fusefuncs[i] + '\0');

  if(size == 0)
    return xattrs.size();

  if(size < xattrs.size())
    return -ERANGE;

  memcpy(list,xattrs.c_str(),xattrs.size());

  return xattrs.size();
}

static
int
_listxattr(Policy::Func::Search  searchFunc,
           const vector<string> &srcmounts,
           const size_t          minfreespace,
           const string         &fusepath,
           char                 *list,
           const size_t          size)
{
#ifndef WITHOUT_XATTR
  int rv;
  vector<string> path;

  rv = searchFunc(srcmounts,fusepath,minfreespace,path);
  if(rv == -1)
    return -errno;

  fs::path::append(path[0],fusepath);

  rv = ::llistxattr(path[0].c_str(),list,size);

  return ((rv == -1) ? -errno : rv);
#else
  return -ENOTSUP;
#endif
}

namespace mergerfs
{
  namespace fuse
  {
    int
    listxattr(const char *fusepath,
              char       *list,
              size_t      size)
    {
      const fuse_context *fc     = fuse_get_context();
      const Config       &config = Config::get(fc);

      if(fusepath == config.controlfile)
        return _listxattr_controlfile(list,size);

      const ugid::Set         ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard readlock(&config.srcmountslock);

      return _listxattr(config.listxattr,
                        config.srcmounts,
                        config.minfreespace,
                        fusepath,
                        list,
                        size);
    }
  }
}
