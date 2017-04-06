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
#include <sstream>

#include <string.h>

#include "config.hpp"
#include "errno.hpp"
#include "fs_base_setxattr.hpp"
#include "fs_path.hpp"
#include "num.hpp"
#include "rv.hpp"
#include "rwlock.hpp"
#include "str.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;
using mergerfs::FuseFunc;
using namespace mergerfs;

static
int
_add_srcmounts(vector<string>           &srcmounts,
               pthread_rwlock_t         &srcmountslock,
               const string             &destmount,
               const string             &values,
               vector<string>::iterator  pos)
{
  vector<string> patterns;
  vector<string> additions;

  str::split(patterns,values,':');
  fs::glob(patterns,additions);
  fs::realpathize(additions);

  if(!additions.empty())
    {
      const rwlock::WriteGuard wrg(&srcmountslock);

      srcmounts.insert(pos,
                       additions.begin(),
                       additions.end());
    }

  return 0;
}

static
int
_erase_srcmounts(vector<string>   &srcmounts,
                 pthread_rwlock_t &srcmountslock,
                 const string     &values)
{
  if(srcmounts.empty())
    return 0;

  vector<string> patterns;

  str::split(patterns,values,':');

  {
    const rwlock::WriteGuard wrg(&srcmountslock);

    str::erase_fnmatches(patterns,srcmounts);
  }

  return 0;
}

static
int
_replace_srcmounts(vector<string>   &srcmounts,
                   pthread_rwlock_t &srcmountslock,
                   const string     &destmount,
                   const string     &values)
{
  vector<string> patterns;
  vector<string> newmounts;

  str::split(patterns,values,':');
  fs::glob(patterns,newmounts);
  fs::realpathize(newmounts);

  {
    const rwlock::WriteGuard wrg(&srcmountslock);

    srcmounts.swap(newmounts);
  }

  return 0;
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
_setxattr_srcmounts(vector<string>   &srcmounts,
                    pthread_rwlock_t &srcmountslock,
                    const string     &destmount,
                    const string     &attrval,
                    const int         flags)
{
  string instruction;
  string values;

  if((flags & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  _split_attrval(attrval,instruction,values);

  if(instruction == "+")
    return _add_srcmounts(srcmounts,srcmountslock,destmount,values,srcmounts.end());
  else if(instruction == "+<")
    return _add_srcmounts(srcmounts,srcmountslock,destmount,values,srcmounts.begin());
  else if(instruction == "+>")
    return _add_srcmounts(srcmounts,srcmountslock,destmount,values,srcmounts.end());
  else if(instruction == "-")
    return _erase_srcmounts(srcmounts,srcmountslock,values);
  else if(instruction == "-<")
    return _erase_srcmounts(srcmounts,srcmountslock,srcmounts.front());
  else if(instruction == "->")
    return _erase_srcmounts(srcmounts,srcmountslock,srcmounts.back());
  else if(instruction == "=")
    return _replace_srcmounts(srcmounts,srcmountslock,destmount,values);
  else if(instruction.empty())
    return _replace_srcmounts(srcmounts,srcmountslock,destmount,values);

  return -EINVAL;
}

static
int
_setxattr_minfreespace(Config       &config,
                       const string &attrval,
                       const int     flags)
{
  int rv;

  if((flags & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  rv = num::to_uint64_t(attrval,config.minfreespace);
  if(rv == -1)
    return -EINVAL;

  return 0;
}

static
int
_setxattr_moveonenospc(Config       &config,
                       const string &attrval,
                       const int     flags)
{
  if((flags & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  if(attrval == "false")
    config.moveonenospc = false;
  else if(attrval == "true")
    config.moveonenospc = true;
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
        return _setxattr_srcmounts(config.srcmounts,
                                   config.srcmountslock,
                                   config.destmount,
                                   attrval,
                                   flags);
      else if(attr[2] == "minfreespace")
        return _setxattr_minfreespace(config,
                                      attrval,
                                      flags);
      else if(attr[2] == "moveonenospc")
        return _setxattr_moveonenospc(config,
                                      attrval,
                                      flags);
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
                    const u_int32_t position,
                    const int     error)
{
  int rv;
  string fullpath;

  fs::path::make(basepath,fusepath,fullpath);

  rv = fs::lsetxattr(fullpath,attrname,attrval,attrvalsize,flags,position);

  return error::calc(rv,error,errno);
}

static
int
_setxattr_loop(const vector<const string*> &basepaths,
               const char                  *fusepath,
               const char                  *attrname,
               const char                  *attrval,
               const size_t                 attrvalsize,
               const u_int32_t               position,
               const int                    flags)
{
  int error;

  error = -1;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      error = _setxattr_loop_core(basepaths[i],fusepath,
                                  attrname,attrval,attrvalsize,flags,position,
                                  error);
    }

  return -error;
}

static
int
_setxattr(Policy::Func::Action  actionFunc,
          const vector<string> &srcmounts,
          const uint64_t        minfreespace,
          const char           *fusepath,
          const char           *attrname,
          const char           *attrval,
          const size_t          attrvalsize,
          const u_int32_t        position,
          const int             flags)
{
  int rv;
  vector<const string*> basepaths;

  rv = actionFunc(srcmounts,fusepath,minfreespace,basepaths);
  if(rv == -1)
    return -errno;

  return _setxattr_loop(basepaths,fusepath,attrname,attrval,attrvalsize,position,flags);
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
#ifdef __APPLE__
             int         flags,
             u_int32_t   position)
    {
#else
             int         flags)
    {
      u_int32_t position = 0;
#endif
      const fuse_context *fc     = fuse_get_context();
      const Config       &config = Config::get(fc);

      if(fusepath == config.controlfile)
        return _setxattr_controlfile(Config::get_writable(),
                                     attrname,
                                     string(attrval,attrvalsize),
                                     flags);

      const ugid::Set         ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard readlock(&config.srcmountslock);

      return _setxattr(config.setxattr,
                       config.srcmounts,
                       config.minfreespace,
                       fusepath,
                       attrname,
                       attrval,
                       attrvalsize,
                       position,
                       flags);
    }
  }
}
