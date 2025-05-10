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
#include "fs_glob.hpp"
#include "fs_lsetxattr.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "num.hpp"
#include "policy_rv.hpp"
#include "str.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>
#include <vector>

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
  setxattr_controlfile(const string &attrname_,
                       const string &attrval_,
                       const int     flags_)
  {
    int rv;
    string key;
    Config::Write cfg;

    if(!str::startswith(attrname_,"user.mergerfs."))
      return -ENOATTR;

    key = &attrname_[14];

    if(cfg->has_key(key) == false)
      return -ENOATTR;

    if((flags_ & XATTR_CREATE) == XATTR_CREATE)
      return -EEXIST;

    rv = cfg->set(key,attrval_);
    if(rv < 0)
      return rv;

    fs::statvfs_cache_timeout(cfg->cache_statfs);

    return rv;
  }

  static
  void
  setxattr_loop_core(const string &basepath_,
                     const char   *fusepath_,
                     const char   *attrname_,
                     const char   *attrval_,
                     const size_t  attrvalsize_,
                     const int     flags_,
                     PolicyRV     *prv_)
  {
    string fullpath;

    fullpath = fs::path::make(basepath_,fusepath_);

    errno = 0;
    fs::lsetxattr(fullpath,attrname_,attrval_,attrvalsize_,flags_);

    prv_->insert(errno,basepath_);
  }

  static
  void
  setxattr_loop(const std::vector<Branch*> &branches_,
                const char                 *fusepath_,
                const char                 *attrname_,
                const char                 *attrval_,
                const size_t                attrvalsize_,
                const int                   flags_,
                PolicyRV                   *prv_)
  {
    for(auto &branch : branches_)
      {
        l::setxattr_loop_core(branch->path,
                              fusepath_,
                              attrname_,
                              attrval_,attrvalsize_,
                              flags_,
                              prv_);
      }
  }

  static
  int
  setxattr(const Policy::Action &setxattrPolicy_,
           const Policy::Search &getxattrPolicy_,
           const Branches       &branches_,
           const char           *fusepath_,
           const char           *attrname_,
           const char           *attrval_,
           const size_t          attrvalsize_,
           const int             flags_)
  {
    int rv;
    PolicyRV prv;
    std::vector<Branch*> branches;

    rv = setxattrPolicy_(branches_,fusepath_,branches);
    if(rv == -1)
      return -errno;

    l::setxattr_loop(branches,fusepath_,attrname_,attrval_,attrvalsize_,flags_,&prv);
    if(prv.errors.empty())
      return 0;
    if(prv.successes.empty())
      return prv.errors[0].rv;

    branches.clear();
    rv = getxattrPolicy_(branches_,fusepath_,branches);
    if(rv == -1)
      return -errno;

    return prv.get_error(branches[0]->path);
  }

  int
  setxattr(const char *fusepath_,
           const char *attrname_,
           const char *attrval_,
           size_t      attrvalsize_,
           int         flags_)
  {
    Config::Read cfg;

    if((cfg->security_capability == false) &&
       l::is_attrname_security_capability(attrname_))
      return -ENOATTR;

    if(cfg->xattr.to_int())
      return -cfg->xattr.to_int();

    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::setxattr(cfg->func.setxattr.policy,
                       cfg->func.getxattr.policy,
                       cfg->branches,
                       fusepath_,
                       attrname_,
                       attrval_,
                       attrvalsize_,
                       flags_);
  }
}

namespace FUSE
{
  int
  setxattr(const char *fusepath_,
           const char *attrname_,
           const char *attrval_,
           size_t      attrvalsize_,
           int         flags_)
  {
    if(fusepath_ == CONTROLFILE)
      return l::setxattr_controlfile(attrname_,
                                     string(attrval_,attrvalsize_),
                                     flags_);

    return l::setxattr(fusepath_,attrname_,attrval_,attrvalsize_,flags_);
  }
}
