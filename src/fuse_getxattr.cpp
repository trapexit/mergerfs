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
#include "fs_findallfiles.hpp"
#include "fs_lgetxattr.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "str.hpp"
#include "ugid.hpp"
#include "version.hpp"

#include "fuse.h"

#include <algorithm>
#include <sstream>
#include <string>

#include <stdio.h>
#include <string.h>

static const char SECURITY_CAPABILITY[] = "security.capability";

using std::string;


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
  getxattr_controlfile(Config::Read &cfg_,
                       const char   *attrname_,
                       char         *buf_,
                       const size_t  count_)
  {
    int rv;
    size_t len;
    string key;
    string val;
    StrVec attr;

    if(!str::startswith(attrname_,"user.mergerfs."))
      return -ENOATTR;

    key = &attrname_[14];
    rv = cfg_->get(key,&val);
    if(rv < 0)
      return rv;

    len = val.size();

    if(count_ == 0)
      return len;

    if(count_ < len)
      return -ERANGE;

    memcpy(buf_,val.c_str(),len);

    return (int)len;
  }

  static
  int
  getxattr_from_string(char         *destbuf_,
                       const size_t  destbufsize_,
                       const string &src_)
  {
    const size_t srcbufsize = src_.size();

    if(destbufsize_ == 0)
      return srcbufsize;

    if(srcbufsize > destbufsize_)
      return -ERANGE;

    memcpy(destbuf_,src_.data(),srcbufsize);

    return srcbufsize;
  }

  static
  int
  getxattr_user_mergerfs_allpaths(const Branches::CPtr &branches_,
                                  const char           *fusepath_,
                                  char                 *buf_,
                                  const size_t          count_)
  {
    string concated;
    StrVec paths;
    StrVec branches;

    branches_->to_paths(branches);

    fs::findallfiles(branches,fusepath_,&paths);

    concated = str::join(paths,'\0');

    return l::getxattr_from_string(buf_,count_,concated);
  }

  static
  int
  getxattr_user_mergerfs(const string         &basepath_,
                         const char           *fusepath_,
                         const string         &fullpath_,
                         const Branches       &branches_,
                         const char           *attrname_,
                         char                 *buf_,
                         const size_t          count_)
  {
    StrVec attr;

    str::split(attrname_,'.',&attr);

    if(attr[2] == "basepath")
      return l::getxattr_from_string(buf_,count_,basepath_);
    else if(attr[2] ==  "relpath")
      return l::getxattr_from_string(buf_,count_,fusepath_);
    else if(attr[2] == "fullpath")
      return l::getxattr_from_string(buf_,count_,fullpath_);
    else if(attr[2] == "allpaths")
      return l::getxattr_user_mergerfs_allpaths(branches_,fusepath_,buf_,count_);

    return -ENOATTR;
  }

  static
  int
  getxattr(const Policy::Search &searchFunc_,
           const Branches       &branches_,
           const char           *fusepath_,
           const char           *attrname_,
           char                 *buf_,
           const size_t          count_)
  {
    int rv;
    string fullpath;
    StrVec basepaths;

    rv = searchFunc_(branches_,fusepath_,&basepaths);
    if(rv == -1)
      return -errno;

    fullpath = fs::path::make(basepaths[0],fusepath_);

    if(str::startswith(attrname_,"user.mergerfs."))
      return l::getxattr_user_mergerfs(basepaths[0],
                                       fusepath_,
                                       fullpath,
                                       branches_,
                                       attrname_,
                                       buf_,
                                       count_);

    return l::lgetxattr(fullpath,attrname_,buf_,count_);
  }
}

namespace FUSE
{
  int
  getxattr(const char *fusepath_,
           const char *attrname_,
           char       *buf_,
           size_t      count_)
  {
    Config::Read cfg;

    if(fusepath_ == CONTROLFILE)
      return l::getxattr_controlfile(cfg,
                                     attrname_,
                                     buf_,
                                     count_);

    if((cfg->security_capability == false) &&
       l::is_attrname_security_capability(attrname_))
      return -ENOATTR;

    if(cfg->xattr.to_int())
      return -cfg->xattr.to_int();

    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::getxattr(cfg->func.getxattr.policy,
                       cfg->branches,
                       fusepath_,
                       attrname_,
                       buf_,
                       count_);
  }
}
