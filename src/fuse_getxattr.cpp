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

#include "fuse_getxattr.hpp"

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
#include <cstring>
#include <sstream>
#include <string>

#include <stdio.h>

static const char SECURITY_CAPABILITY[] = "security.capability";


static
bool
_is_attrname_security_capability(const char *attrname_)
{
  return (strcmp(attrname_,SECURITY_CAPABILITY) == 0);
}

static
int
_getxattr_ctrl_file(Config       &cfg_,
                    const char   *attrname_,
                    char         *buf_,
                    const size_t  count_)
{
  int rv;
  std::string key;
  std::string val;

  if(!Config::is_mergerfs_xattr(attrname_))
    return -ENOATTR;

  key = Config::prune_ctrl_xattr(attrname_);
  rv = cfg_.get(key,&val);
  if(rv < 0)
    return rv;

  if(count_ == 0)
    return val.size();

  if(count_ < val.size())
    return -ERANGE;

  memcpy(buf_,val.c_str(),val.size());

  return (int)val.size();
}

static
int
_getxattr_from_string(char              *destbuf_,
                      const size_t       destbufsize_,
                      const std::string &src_)
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
_getxattr_user_mergerfs_allpaths(const Branches::Ptr &branches_,
                                 const fs::path      &fusepath_,
                                 char                *buf_,
                                 const size_t         count_)
{
  std::string concated;
  StrVec paths;
  StrVec branches;

  branches_->to_paths(branches);

  fs::findallfiles(branches,fusepath_,&paths);

  concated = str::join(paths,'\0');

  return ::_getxattr_from_string(buf_,count_,concated);
}

static
int
_getxattr_user_mergerfs(const fs::path &basepath_,
                        const fs::path &fusepath_,
                        const fs::path &fullpath_,
                        const Branches &branches_,
                        const char     *attrname_,
                        char           *buf_,
                        const size_t    count_)
{
  std::string key;

  key = Config::prune_ctrl_xattr(attrname_);

  if(key == "basepath")
    return ::_getxattr_from_string(buf_,count_,basepath_);
  if(key == "relpath")
    return ::_getxattr_from_string(buf_,count_,fusepath_);
  if(key == "fullpath")
    return ::_getxattr_from_string(buf_,count_,fullpath_);
  if(key == "allpaths")
    return ::_getxattr_user_mergerfs_allpaths(branches_,fusepath_,buf_,count_);

  return -ENOATTR;
}

static
int
_getxattr(const Policy::Search &searchFunc_,
          const Branches       &branches_,
          const fs::path       &fusepath_,
          const char           *attrname_,
          char                 *buf_,
          const size_t          count_)
{
  int rv;
  fs::path fullpath;
  std::vector<Branch*> branches;

  rv = searchFunc_(branches_,fusepath_,branches);
  if(rv < 0)
    return rv;

  fullpath = branches[0]->path / fusepath_;

  if(Config::is_mergerfs_xattr(attrname_))
    return ::_getxattr_user_mergerfs(branches[0]->path,
                                     fusepath_,
                                     fullpath,
                                     branches_,
                                     attrname_,
                                     buf_,
                                     count_);

  return fs::lgetxattr(fullpath,attrname_,buf_,count_);
}

int
FUSE::getxattr(const fuse_req_ctx_t *ctx_,
               const char           *fusepath_,
               const char           *attrname_,
               char                 *attrvalue_,
               size_t                attrvalue_size_)
{
  const fs::path fusepath{fusepath_};

  if(Config::is_ctrl_file(fusepath))
    return ::_getxattr_ctrl_file(cfg,
                                 attrname_,
                                 attrvalue_,
                                 attrvalue_size_);

  if((cfg.security_capability == false) &&
     ::_is_attrname_security_capability(attrname_))
    return -ENOATTR;

  if(cfg.xattr.to_int())
    return -cfg.xattr.to_int();

  const ugid::Set ugid(ctx_->uid,ctx_->gid);

  return ::_getxattr(cfg.func.getxattr.policy,
                     cfg.branches,
                     fusepath,
                     attrname_,
                     attrvalue_,
                     attrvalue_size_);
}
