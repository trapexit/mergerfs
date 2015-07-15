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
#include <set>
#include <algorithm>

#include <stdio.h>
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
using std::set;
using namespace mergerfs;
using namespace mergerfs::config;

static
void
_getxattr_controlfile_fusefunc_policy(const Config &config,
                                      const char   *attrbasename,
                                      string       &attrvalue)
{
  FuseFunc fusefunc;

  fusefunc = FuseFunc::find(attrbasename);
  if(fusefunc != FuseFunc::invalid)
    attrvalue = (std::string)*config.policies[(FuseFunc::Enum::Type)*fusefunc];
}

static
void
_getxattr_controlfile_category_policy(const Config &config,
                                      const char   *attrbasename,
                                      string       &attrvalue)
{
  Category cat;

  cat = Category::find(attrbasename);
  if(cat != Category::invalid)
    {
      vector<string> policies;
      for(int i = FuseFunc::Enum::BEGIN; i < FuseFunc::Enum::END; i++)
        {
          if(cat == (Category::Enum::Type)*FuseFunc::fusefuncs[i])
            policies.push_back(*config.policies[i]);
        }

      std::sort(policies.begin(),policies.end());
      policies.erase(std::unique(policies.begin(),policies.end()),
                     policies.end());
      attrvalue = str::join(policies,',');
    }
}

static
void
_getxattr_controlfile_srcmounts(const Config &config,
                                string       &attrvalue)
{
  attrvalue = str::join(config.srcmounts,':');
}

static
void
_getxattr_controlfile_minfreespace(const Config &config,
                                   string       &attrvalue)
{
  char buf[64];

  snprintf(buf,sizeof(buf),"%li",config.minfreespace);

  attrvalue = buf;
}

static
int
_getxattr_controlfile(const Config &config,
                      const char   *attrname,
                      char         *buf,
                      const size_t  count)
{
  size_t len;
  string attrvalue;
  const char *attrbasename = &attrname[sizeof("user.mergerfs.")-1];

  if(strncmp("user.mergerfs.",attrname,sizeof("user.mergerfs.")-1))
    return -ENOATTR;

  if(!strcmp("srcmounts",attrbasename))
    _getxattr_controlfile_srcmounts(config,attrvalue);
  else if(!strcmp("minfreespace",attrbasename))
    _getxattr_controlfile_minfreespace(config,attrvalue);
  else if(!strncmp("category.",attrbasename,sizeof("category.")-1))
    _getxattr_controlfile_category_policy(config,&attrbasename[sizeof("category.")-1],attrvalue);
  else if(!strncmp("func.",attrbasename,sizeof("func.")-1))
    _getxattr_controlfile_fusefunc_policy(config,&attrbasename[sizeof("func.")-1],attrvalue);

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
_getxattr_user_mergerfs(const string         &basepath,
                        const string         &fusepath,
                        const string         &fullpath,
                        const vector<string> &srcmounts,
                        const char           *attrname,
                        char                 *buf,
                        const size_t          count)
{
  const char *attrbasename = &attrname[sizeof("user.mergerfs")];

  if(!strcmp(attrbasename,"basepath"))
    return ::_getxattr_from_string(buf,count,basepath);
  else if(!strcmp(attrbasename,"fullpath"))
    return ::_getxattr_from_string(buf,count,fullpath);
  else if(!strcmp(attrbasename,"relpath"))
    return ::_getxattr_from_string(buf,count,fusepath);
  else if(!strcmp(attrbasename,"allpaths"))
    return ::_getxattr_user_mergerfs_allpaths(srcmounts,fusepath,buf,count);

  return ::lgetxattr(fullpath.c_str(),attrname,buf,count);
}

static
int
_getxattr(Policy::Func::Search  searchFunc,
          const vector<string> &srcmounts,
          const size_t          minfreespace,
          const string         &fusepath,
          const char           *attrname,
          char                 *buf,
          const size_t          count)
{
#ifndef WITHOUT_XATTR
  int rv;
  vector<string> basepath;
  string fullpath;

  rv = searchFunc(srcmounts,fusepath,minfreespace,basepath);
  if(rv == -1)
    return -errno;

  fullpath = fs::path::make(basepath[0],fusepath);

  if(!strncmp("user.mergerfs.",attrname,sizeof("user.mergerfs.")-1))
    rv = _getxattr_user_mergerfs(basepath[0],fusepath,fullpath,srcmounts,attrname,buf,count);
  else
    rv = ::lgetxattr(fullpath.c_str(),attrname,buf,count);

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

      return _getxattr(config.getxattr,
                       config.srcmounts,
                       config.minfreespace,
                       fusepath,
                       attrname,
                       buf,
                       count);
    }
  }
}
