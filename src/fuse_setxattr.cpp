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

#include "fuse_setxattr.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fs_glob.hpp"
#include "fs_lsetxattr.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "gidcache.hpp"
#include "num.hpp"
#include "policy_rv.hpp"
#include "str.hpp"
#include "ugid.hpp"
#include "syslog.hpp"

#include "fuse.h"

#include <string>
#include <vector>

#include <cstring>

static const char SECURITY_CAPABILITY[] = "security.capability";


static
int
_setxattr_cmd_xattr(const std::string_view &attrname_,
                    const std::string_view &attrval_)
{
  std::string_view cmd;

  cmd = Config::prune_cmd_xattr(attrname_);

  SysLog::info("command requested: {}={}",cmd,attrval_);

  if(cmd == "gc")
    return (fuse_gc(),0);
  if(cmd == "gc1")
    return (fuse_gc1(),0);
  if(cmd == "invalidate-all-nodes")
    return (fuse_invalidate_all_nodes(),0);
  if(cmd == "invalidate-gid-cache")
    return (GIDCache::invalidate_all(),0);
  if(cmd == "clear-gid-cache")
    return (GIDCache::clear_all(),0);

  return -ENOATTR;
}

static
bool
_is_attrname_security_capability(const char *attrname_)
{
  return str::eq(attrname_,SECURITY_CAPABILITY);
}

static
int
_setxattr_ctrl_file(const char *attrname_,
                    const char *attrval_,
                    size_t      attrvalsize_,
                    const int   flags_)
{
  int rv;
  std::string key;
  Config::Write cfg;

  if(!Config::is_mergerfs_xattr(attrname_))
    return -ENOATTR;

  if(Config::is_cmd_xattr(attrname_))
    return ::_setxattr_cmd_xattr(attrname_,
                                 std::string_view{attrval_,attrvalsize_});

  key = Config::prune_ctrl_xattr(attrname_);

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
_setxattr_loop_core(const std::string &basepath_,
                    const char        *fusepath_,
                    const char        *attrname_,
                    const char        *attrval_,
                    const size_t       attrvalsize_,
                    const int          flags_,
                    PolicyRV          *prv_)
{
  std::string fullpath;

  fullpath = fs::path::make(basepath_,fusepath_);

  errno = 0;
  fs::lsetxattr(fullpath,attrname_,attrval_,attrvalsize_,flags_);

  prv_->insert(errno,basepath_);
}

static
void
_setxattr_loop(const std::vector<Branch*> &branches_,
               const char                 *fusepath_,
               const char                 *attrname_,
               const char                 *attrval_,
               const size_t                attrvalsize_,
               const int                   flags_,
               PolicyRV                   *prv_)
{
  for(auto &branch : branches_)
    {
      ::_setxattr_loop_core(branch->path,
                            fusepath_,
                            attrname_,
                            attrval_,attrvalsize_,
                            flags_,
                            prv_);
    }
}

static
int
_setxattr(const Policy::Action &setxattrPolicy_,
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
  if(rv < 0)
    return rv;

  ::_setxattr_loop(branches,fusepath_,attrname_,attrval_,attrvalsize_,flags_,&prv);
  if(prv.errors.empty())
    return 0;
  if(prv.successes.empty())
    return prv.errors[0].rv;

  branches.clear();
  rv = getxattrPolicy_(branches_,fusepath_,branches);
  if(rv < 0)
    return rv;

  return prv.get_error(branches[0]->path);
}

static
int
_setxattr(const char *fusepath_,
          const char *attrname_,
          const char *attrval_,
          size_t      attrvalsize_,
          int         flags_)
{
  Config::Read cfg;

  if((cfg->security_capability == false) &&
     ::_is_attrname_security_capability(attrname_))
    return -ENOATTR;

  if(cfg->xattr.to_int())
    return -cfg->xattr.to_int();

  const fuse_context *fc = fuse_get_context();
  const ugid::Set     ugid(fc->uid,fc->gid);

  return ::_setxattr(cfg->func.setxattr.policy,
                     cfg->func.getxattr.policy,
                     cfg->branches,
                     fusepath_,
                     attrname_,
                     attrval_,
                     attrvalsize_,
                     flags_);
}


int
FUSE::setxattr(const char *fusepath_,
               const char *attrname_,
               const char *attrval_,
               size_t      attrvalsize_,
               int         flags_)
{
  if(Config::is_ctrl_file(fusepath_))
    return ::_setxattr_ctrl_file(attrname_,
                                 attrval_,
                                 attrvalsize_,
                                 flags_);

  return ::_setxattr(fusepath_,attrname_,attrval_,attrvalsize_,flags_);
}
