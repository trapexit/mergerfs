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

#include <fuse.h>

#include <string.h>

#include <sstream>
#include <string>
#include <vector>

#include "config.hpp"
#include "errno.hpp"
#include "fs_base_setxattr.hpp"
#include "fs_glob.hpp"
#include "fs_path.hpp"
#include "num.hpp"
#include "rv.hpp"
#include "rwlock.hpp"
#include "str.hpp"
#include "ugid.hpp"

static const char SECURITY_CAPABILITY[] = "security.capability";

using std::string;
using std::vector;
using mergerfs::Policy;
using mergerfs::FuseFunc;
using namespace mergerfs;

static
bool
_is_attrname_security_capability(const char *attrname_)
{
  return (strcmp(attrname_,SECURITY_CAPABILITY) == 0);
}

static
void
_split_attrval(const string &attrval,
               string       &instruction,
               string       &values)
{
  size_t offset;

  offset = attrval.find_first_of('/');
  instruction = attrval.substr(0,offset);
  if(offset != string::npos)
    values = attrval.substr(offset);
}

static
int
_setxattr_srcmounts(const string     &attrval,
                    const int         flags,
                    Branches         &branches_,
                    pthread_rwlock_t &branches_lock)
{
  const rwlock::WriteGuard wrg(&branches_lock);

  string instruction;
  string values;

  if((flags & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  _split_attrval(attrval,instruction,values);

  if(instruction == "+")
    branches_.add_end(values);
  else if(instruction == "+<")
    branches_.add_begin(values);
  else if(instruction == "+>")
    branches_.add_end(values);
  else if(instruction == "-")
    branches_.erase_fnmatch(values);
  else if(instruction == "-<")
    branches_.erase_begin();
  else if(instruction == "->")
    branches_.erase_end();
  else if(instruction == "=")
    branches_.set(values);
  else if(instruction.empty())
    branches_.set(values);
  else
    return -EINVAL;

  return 0;
}

static
int
_setxattr_uint64_t(const string &attrval,
                   const int     flags,
                   uint64_t     &uint)
{
  int rv;

  if((flags & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  rv = num::to_uint64_t(attrval,uint);
  if(rv == -1)
    return -EINVAL;

  return 0;
}

static
int
_setxattr_time_t(const string &attrval,
                 const int     flags,
                 time_t       &time)
{
  int rv;

  if((flags & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  rv = num::to_time_t(attrval,time);
  if(rv == -1)
    return -EINVAL;

  return 0;
}

static
int
_setxattr_bool(const string &attrval,
               const int     flags,
               bool         &value)
{
  if((flags & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  if(attrval == "false")
    value = false;
  else if(attrval == "true")
    value = true;
  else
    return -EINVAL;

  return 0;
}

static
int
_setxattr_xattr(const string &attrval_,
                const int     flags_,
                int           xattr_)
{
  if((flags_ & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  if(attrval_ == "passthrough")
    xattr_ = 0;
  else if(attrval_ == "noattr")
    xattr_ = ENOATTR;
  else if(attrval_ == "nosys")
    xattr_ = ENOSYS;
  else
    return -EINVAL;

  return 0;
}

static
int
_setxattr_statfs(const string         &attrval_,
                 const int             flags_,
                 Config::StatFS::Enum &enum_)
{
  if((flags_ & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  if(attrval_ == "base")
    enum_ = Config::StatFS::BASE;
  else if(attrval_ == "full")
    enum_ = Config::StatFS::FULL;
  else
    return -EINVAL;

  return 0;
}

static
int
_setxattr_statfsignore(const string               &attrval_,
                       const int                   flags_,
                       Config::StatFSIgnore::Enum &enum_)
{
  if((flags_ & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  if(attrval_ == "none")
    enum_ = Config::StatFSIgnore::NONE;
  else if(attrval_ == "ro")
    enum_ = Config::StatFSIgnore::RO;
  else if(attrval_ == "nc")
    enum_ = Config::StatFSIgnore::NC;
  else
    return -EINVAL;

  return 0;
}

static
int
_setxattr_controlfile_func_policy(Config       &config,
                                  const string &funcname,
                                  const string &attrval,
                                  const int     flags)
{
  int rv;

  if((flags & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  rv = config.set_func_policy(funcname,attrval);
  if(rv == -1)
    return -errno;

  return 0;
}

static
int
_setxattr_controlfile_category_policy(Config       &config,
                                      const string &categoryname,
                                      const string &attrval,
                                      const int     flags)
{
  int rv;

  if((flags & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  rv = config.set_category_policy(categoryname,attrval);
  if(rv == -1)
    return -errno;

  return 0;
}

static
int
_setxattr_controlfile(Config       &config,
                      const string &attrname,
                      const string &attrval,
                      const int     flags)
{
  vector<string> attr;

  str::split(attr,attrname,'.');

  switch(attr.size())
    {
    case 3:
      if(attr[2] == "srcmounts")
        return _setxattr_srcmounts(attrval,
                                   flags,
                                   config.branches,
                                   config.branches_lock);
      else if(attr[2] == "branches")
        return _setxattr_srcmounts(attrval,
                                   flags,
                                   config.branches,
                                   config.branches_lock);
      else if(attr[2] == "minfreespace")
        return _setxattr_uint64_t(attrval,
                                  flags,
                                  config.minfreespace);
      else if(attr[2] == "moveonenospc")
        return _setxattr_bool(attrval,
                              flags,
                              config.moveonenospc);
      else if(attr[2] == "dropcacheonclose")
        return _setxattr_bool(attrval,
                              flags,
                              config.dropcacheonclose);
      else if(attr[2] == "symlinkify")
        return _setxattr_bool(attrval,
                              flags,
                              config.symlinkify);
      else if(attr[2] == "symlinkify_timeout")
        return _setxattr_time_t(attrval,
                                flags,
                                config.symlinkify_timeout);
      else if(attr[2] == "ignorepponrename")
        return _setxattr_bool(attrval,
                              flags,
                              config.ignorepponrename);
      else if(attr[2] == "security_capability")
        return _setxattr_bool(attrval,
                              flags,
                              config.security_capability);
      else if(attr[2] == "xattr")
        return _setxattr_xattr(attrval,
                               flags,
                               config.xattr);
      else if(attr[2] == "link_cow")
        return _setxattr_bool(attrval,
                              flags,
                              config.link_cow);
      else if(attr[2] == "statfs")
        return _setxattr_statfs(attrval,
                                flags,
                                config.statfs);
      else if(attr[2] == "statfs_ignore")
        return _setxattr_statfsignore(attrval,
                                      flags,
                                      config.statfs_ignore);
      break;

    case 4:
      if(attr[2] == "category")
        return _setxattr_controlfile_category_policy(config,
                                                     attr[3],
                                                     attrval,
                                                     flags);
      else if(attr[2] == "func")
        return _setxattr_controlfile_func_policy(config,
                                                 attr[3],
                                                 attrval,
                                                 flags);
      break;

    default:
      break;
    }

  return -EINVAL;
}

static
int
_setxattr_loop_core(const string *basepath,
                    const char   *fusepath,
                    const char   *attrname,
                    const char   *attrval,
                    const size_t  attrvalsize,
                    const int     flags,
                    const int     error)
{
  int rv;
  string fullpath;

  fs::path::make(basepath,fusepath,fullpath);

  rv = fs::lsetxattr(fullpath,attrname,attrval,attrvalsize,flags);

  return error::calc(rv,error,errno);
}

static
int
_setxattr_loop(const vector<const string*> &basepaths,
               const char                  *fusepath,
               const char                  *attrname,
               const char                  *attrval,
               const size_t                 attrvalsize,
               const int                    flags)
{
  int error;

  error = -1;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      error = _setxattr_loop_core(basepaths[i],fusepath,
                                  attrname,attrval,attrvalsize,flags,
                                  error);
    }

  return -error;
}

static
int
_setxattr(Policy::Func::Action  actionFunc,
          const Branches       &branches_,
          const uint64_t        minfreespace,
          const char           *fusepath,
          const char           *attrname,
          const char           *attrval,
          const size_t          attrvalsize,
          const int             flags)
{
  int rv;
  vector<const string*> basepaths;

  rv = actionFunc(branches_,fusepath,minfreespace,basepaths);
  if(rv == -1)
    return -errno;

  return _setxattr_loop(basepaths,fusepath,attrname,attrval,attrvalsize,flags);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    setxattr(const char *fusepath,
             const char *attrname,
             const char *attrval,
             size_t      attrvalsize,
             int         flags)
    {
      const fuse_context *fc     = fuse_get_context();
      const Config       &config = Config::get(fc);

      if(fusepath == config.controlfile)
        return _setxattr_controlfile(Config::get_writable(),
                                     attrname,
                                     string(attrval,attrvalsize),
                                     flags);

      if((config.security_capability == false) &&
         _is_attrname_security_capability(attrname))
        return -ENOATTR;

      if(config.xattr)
        return -config.xattr;

      const ugid::Set         ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard readlock(&config.branches_lock);

      return _setxattr(config.setxattr,
                       config.branches,
                       config.minfreespace,
                       fusepath,
                       attrname,
                       attrval,
                       attrvalsize,
                       flags);
    }
  }
}
