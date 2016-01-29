/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <fuse.h>

#include <string>
#include <vector>
#include <set>
#include <algorithm>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.hpp"
#include "fs_path.hpp"
#include "rwlock.hpp"
#include "str.hpp"
#include "ugid.hpp"
#include "version.hpp"
#include "xattr.hpp"

using std::string;
using std::vector;
using std::set;
using namespace mergerfs;

static
int
_lgetxattr(const string &path,
           const char   *attrname,
           void         *value,
           const size_t  size)
{
#ifndef WITHOUT_XATTR
  int rv;

  rv = ::lgetxattr(path.c_str(),attrname,value,size);

  return ((rv == -1) ? -errno : rv);
#else
  return -ENOTSUP;
#endif
}

static
void
_getxattr_controlfile_fusefunc_policy(const Config &config,
                                      const string &attr,
                                      string       &attrvalue)
{
  FuseFunc fusefunc;

  fusefunc = FuseFunc::find(attr);
  if(fusefunc != FuseFunc::invalid)
    attrvalue = (std::string)*config.policies[(FuseFunc::Enum::Type)*fusefunc];
}

static
void
_getxattr_controlfile_category_policy(const Config &config,
                                      const string &attr,
                                      string       &attrvalue)
{
  Category cat;

  cat = Category::find(attr);
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
  unsigned long long minfreespace;

  minfreespace = (unsigned long long)config.minfreespace;
  snprintf(buf,sizeof(buf),"%llu",minfreespace);

  attrvalue = buf;
}

static
void
_getxattr_controlfile_moveonenospc(const Config &config,
                                   string       &attrvalue)
{
  attrvalue = (config.moveonenospc ? "true" : "false");
}

static
void
_getxattr_controlfile_policies(const Config &config,
                               string       &attrvalue)
{
  size_t i = Policy::Enum::begin();

  attrvalue = (string)Policy::policies[i];
  for(i++; i < Policy::Enum::end(); i++)
    attrvalue += ',' + (string)Policy::policies[i];
}

static
void
_getxattr_controlfile_version(string &attrvalue)
{
  attrvalue = MERGERFS_VERSION;
}

static
void
_getxattr_pid(string &attrvalue)
{
  int pid;
  char buf[32];

  pid = getpid();
  snprintf(buf,sizeof(buf),"%d",pid);

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
  vector<string> attr;

  str::split(attr,attrname,'.');
  if((attr[0] != "user") || (attr[1] != "mergerfs"))
    return -ENOATTR;

  switch(attr.size())
    {
    case 3:
      if(attr[2] == "srcmounts")
        _getxattr_controlfile_srcmounts(config,attrvalue);
      else if(attr[2] == "minfreespace")
        _getxattr_controlfile_minfreespace(config,attrvalue);
      else if(attr[2] == "moveonenospc")
        _getxattr_controlfile_moveonenospc(config,attrvalue);
      else if(attr[2] == "policies")
        _getxattr_controlfile_policies(config,attrvalue);
      else if(attr[2] == "version")
        _getxattr_controlfile_version(attrvalue);
      else if(attr[2] == "pid")
        _getxattr_pid(attrvalue);
      break;

    case 4:
      if(attr[2] == "category")
        _getxattr_controlfile_category_policy(config,attr[3],attrvalue);
      else if(attr[2] == "func")
        _getxattr_controlfile_fusefunc_policy(config,attr[3],attrvalue);
      break;
    }

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
    return -ERANGE;

  memcpy(destbuf,src.data(),srcbufsize);

  return srcbufsize;
}

static
int
_getxattr_user_mergerfs_allpaths(const vector<string> &srcmounts,
                                 const char           *fusepath,
                                 char                 *buf,
                                 const size_t          count)
{
  string concated;
  vector<string> paths;

  fs::findallfiles(srcmounts,fusepath,paths);

  concated = str::join(paths,'\0');

  return _getxattr_from_string(buf,count,concated);
}

static
int
_getxattr_user_mergerfs(const string         &basepath,
                        const char           *fusepath,
                        const string         &fullpath,
                        const vector<string> &srcmounts,
                        const char           *attrname,
                        char                 *buf,
                        const size_t          count)
{
  vector<string> attr;

  str::split(attr,attrname,'.');

  if(attr[2] == "basepath")
    return _getxattr_from_string(buf,count,basepath);
  else if(attr[2] ==  "relpath")
    return _getxattr_from_string(buf,count,fusepath);
  else if(attr[2] == "fullpath")
    return _getxattr_from_string(buf,count,fullpath);
  else if(attr[2] == "allpaths")
    return _getxattr_user_mergerfs_allpaths(srcmounts,fusepath,buf,count);

  return -ENOATTR;
}

static
int
_getxattr(Policy::Func::Search  searchFunc,
          const vector<string> &srcmounts,
          const size_t          minfreespace,
          const char           *fusepath,
          const char           *attrname,
          char                 *buf,
          const size_t          count)
{
  int rv;
  string fullpath;
  vector<const string*> basepaths;

  rv = searchFunc(srcmounts,fusepath,minfreespace,basepaths);
  if(rv == -1)
    return -errno;

  fs::path::make(basepaths[0],fusepath,fullpath);

  if(str::isprefix(attrname,"user.mergerfs."))
    rv = _getxattr_user_mergerfs(*basepaths[0],fusepath,fullpath,srcmounts,attrname,buf,count);
  else
    rv = _lgetxattr(fullpath,attrname,buf,count);

  return rv;
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
      const fuse_context *fc     = fuse_get_context();
      const Config       &config = Config::get(fc);

      if(fusepath == config.controlfile)
        return _getxattr_controlfile(config,
                                     attrname,
                                     buf,
                                     count);

      const ugid::Set         ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard readlock(&config.srcmountslock);

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
