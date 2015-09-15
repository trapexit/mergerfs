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
#include <sstream>

#include <errno.h>
#include <sys/types.h>
#include <string.h>

#include "config.hpp"
#include "fs.hpp"
#include "ugid.hpp"
#include "rwlock.hpp"
#include "xattr.hpp"
#include "str.hpp"
#include "num.hpp"

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

  rv = num::to_size_t(attrval,config.minfreespace);
  if(rv == -1)
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
_setxattr(Policy::Func::Action  actionFunc,
          const vector<string> &srcmounts,
          const size_t          minfreespace,
          const string         &fusepath,
          const char           *attrname,
          const char           *attrval,
          const size_t          attrvalsize,
          const int             flags)
{
#ifndef WITHOUT_XATTR
  int rv;
  int error;
  vector<string> paths;

  rv = actionFunc(srcmounts,fusepath,minfreespace,paths);
  if(rv == -1)
    return -errno;

  error = 0;
  for(size_t i = 0, ei = paths.size(); i != ei; i++)
    {
      fs::path::append(paths[i],fusepath);

      rv = ::lsetxattr(paths[i].c_str(),attrname,attrval,attrvalsize,flags);
      if(rv == -1)
        error = errno;
    }

  return -error;
#else
  return -ENOTSUP;
#endif
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

      const ugid::Set         ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard readlock(&config.srcmountslock);

      return _setxattr(config.setxattr,
                       config.srcmounts,
                       config.minfreespace,
                       fusepath,
                       attrname,
                       attrval,
                       attrvalsize,
                       flags);
    }
  }
}
