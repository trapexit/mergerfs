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
#include <string.h>

#include "config.hpp"
#include "fs.hpp"
#include "ugid.hpp"
#include "rwlock.hpp"
#include "xattr.hpp"
#include "str.hpp"

using std::string;
using std::vector;
using namespace mergerfs::config;

static
int
_getxattr_controlfile(const Config &config,
                      const string &attrname,
                      char         *buf,
                      const size_t  count)
{
  size_t len;
  string attrvalue;

  if(attrname == "user.mergerfs.create")
    attrvalue = (std::string)*config.create;
  else if(attrname == "user.mergerfs.search")
    attrvalue = (std::string)*config.search;
  else if(attrname == "user.mergerfs.srcmounts")
    attrvalue = str::join(config.srcmounts,':');

  if(attrvalue.empty())
    return -ENOATTR;

  len = attrvalue.size();

  if(count == 0)
    return len;

  if(count < len)
    return -ERANGE;

  memcpy(buf,attrvalue.c_str(),len);

  return (int)len;
}

static
int
_getxattr_from_string(char         *destbuf,
                      const size_t  destbufsize,
                      const string &src)
{
  const size_t srcbufsize = src.size();

  if(destbufsize == 0)
    return srcbufsize;

  if(srcbufsize > destbufsize)
    return (errno = ERANGE, -1);

  memcpy(destbuf,src.data(),srcbufsize);

  return srcbufsize;
}

static
int
_getxattr_user_mergerfs_allpaths(const vector<string> &srcmounts,
                                 const string         &fusepath,
                                 char                 *buf,
                                 const size_t          count)
{
  string concated;
  vector<string> paths;

  fs::findallfiles(srcmounts,fusepath,paths);

  concated = str::join(paths,'\0');

  return ::_getxattr_from_string(buf,count,concated);
}

static
int
_getxattr_user_mergerfs(const fs::Path       &path,
                        const vector<string> &srcmounts,
                        const string         &fusepath,
                        const char           *attrname,
                        char                 *buf,
                        const size_t          count)
{
  const char *attrbasename = &attrname[sizeof("user.mergerfs")];

  if(!strcmp(attrbasename,"basepath"))
    return ::_getxattr_from_string(buf,count,path.base);
  else if(!strcmp(attrbasename,"fullpath"))
    return ::_getxattr_from_string(buf,count,path.full);
  else if(!strcmp(attrbasename,"relpath"))
    return ::_getxattr_from_string(buf,count,fusepath);
  else if(!strcmp(attrbasename,"allpaths"))
    return ::_getxattr_user_mergerfs_allpaths(srcmounts,fusepath,buf,count);

  return (errno=ENOATTR,-1);
}

static
int
_getxattr(const fs::SearchFunc  searchFunc,
          const vector<string> &srcmounts,
          const string         &fusepath,
          const char           *attrname,
          char                 *buf,
          const size_t          count)
{
#ifndef WITHOUT_XATTR
  int rv;
  fs::Path path;

  rv = searchFunc(srcmounts,fusepath,path);
  if(rv == -1)
    return -errno;

  if(!strncmp("user.mergerfs.",attrname,sizeof("user.mergerfs.")-1))
    rv = _getxattr_user_mergerfs(path,srcmounts,fusepath,attrname,buf,count);

  if(rv == -1 && errno == ENOATTR)
    rv = ::lgetxattr(path.full.c_str(),attrname,buf,count);

  return ((rv == -1) ? -errno : rv);
#else
  return -ENOTSUP;
#endif
}

namespace mergerfs
{
  namespace getxattr
  {
    int
    getxattr(const char *fusepath,
             const char *attrname,
             char       *buf,
             size_t      count)
    {
      const config::Config &config = config::get();

      if(fusepath == config.controlfile)
        return _getxattr_controlfile(config,
                                     attrname,
                                     buf,
                                     count);

      const struct fuse_context *fc = fuse_get_context();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard    readlock(&config.srcmountslock);

      return _getxattr(*config.search,
                       config.srcmounts,
                       fusepath,
                       attrname,
                       buf,
                       count);
    }
  }
}
