/*
  Copyright (c) 2018, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "config.hpp"
#include "errno.hpp"
#include "fs_base_getxattr.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "str.hpp"
#include "ugid.hpp"
#include "version.hpp"

#include <fuse.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include <stdio.h>
#include <string.h>

static const char SECURITY_CAPABILITY[] = "security.capability";

using std::string;
using std::vector;

namespace l
{
  static
  bool
  is_attrname_security_capability(const char *attrname_)
  {
    return (strcmp(attrname_,SECURITY_CAPABILITY) == 0);
  }

  static
  int
  lgetxattr(const string &path_,
            const char   *attrname_,
            void         *value_,
            const size_t  size_)
  {
    int rv;

    rv = fs::lgetxattr(path_,attrname_,value_,size_);

    return ((rv == -1) ? -errno : rv);
  }

  static
  int
  getxattr_controlfile(const Config &config,
                       const char   *attrname,
                       char         *buf,
                       const size_t  count)
  {
    int rv;
    size_t len;
    string key;
    string val;
    vector<string> attr;

    if(!str::startswith(attrname,"user.mergerfs."))
      return -ENOATTR;

    key = &attrname[14];
    rv = config.get(key,&val);
    if(rv < 0)
      return rv;

    len = val.size();

    if(count == 0)
      return len;

    if(count < len)
      return -ERANGE;

    memcpy(buf,val.c_str(),len);

    return (int)len;
  }

  static
  int
  getxattr_from_string(char         *destbuf,
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
  getxattr_user_mergerfs_allpaths(const Branches &branches_,
                                  const char     *fusepath,
                                  char           *buf,
                                  const size_t    count)
  {
    string concated;
    vector<string> paths;
    vector<string> branches;

    branches_.to_paths(branches);

    fs::findallfiles(branches,fusepath,paths);

    concated = str::join(paths,'\0');

    return l::getxattr_from_string(buf,count,concated);
  }

  static
  int
  getxattr_user_mergerfs(const string         &basepath,
                         const char           *fusepath,
                         const string         &fullpath,
                         const Branches       &branches_,
                         const char           *attrname,
                         char                 *buf,
                         const size_t          count)
  {
    vector<string> attr;

    str::split(attr,attrname,'.');

    if(attr[2] == "basepath")
      return l::getxattr_from_string(buf,count,basepath);
    else if(attr[2] ==  "relpath")
      return l::getxattr_from_string(buf,count,fusepath);
    else if(attr[2] == "fullpath")
      return l::getxattr_from_string(buf,count,fullpath);
    else if(attr[2] == "allpaths")
      return l::getxattr_user_mergerfs_allpaths(branches_,fusepath,buf,count);

    return -ENOATTR;
  }

  static
  int
  getxattr(Policy::Func::Search  searchFunc,
           const Branches       &branches_,
           const size_t          minfreespace,
           const char           *fusepath,
           const char           *attrname,
           char                 *buf,
           const size_t          count)
  {
    int rv;
    string fullpath;
    vector<string> basepaths;

    rv = searchFunc(branches_,fusepath,minfreespace,&basepaths);
    if(rv == -1)
      return -errno;

    fullpath = fs::path::make(basepaths[0],fusepath);

    if(str::startswith(attrname,"user.mergerfs."))
      return l::getxattr_user_mergerfs(basepaths[0],
                                       fusepath,
                                       fullpath,
                                       branches_,
                                       attrname,
                                       buf,
                                       count);

    return l::lgetxattr(fullpath,attrname,buf,count);
  }
}

namespace FUSE
{
  int
  getxattr(const char *fusepath,
           const char *attrname,
           char       *buf,
           size_t      count)
  {
    const Config &config = Config::ro();

    if(fusepath == config.controlfile)
      return l::getxattr_controlfile(config,
                                     attrname,
                                     buf,
                                     count);

    if((config.security_capability == false) &&
       l::is_attrname_security_capability(attrname))
      return -ENOATTR;

    if(config.xattr.to_int())
      return -config.xattr.to_int();

    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::getxattr(config.func.getxattr.policy,
                       config.branches,
                       config.minfreespace,
                       fusepath,
                       attrname,
                       buf,
                       count);
  }
}
