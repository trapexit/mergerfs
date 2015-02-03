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

#include "config.hpp"
#include "fs.hpp"
#include "ugid.hpp"
#include "rwlock.hpp"
#include "xattr.hpp"
#include "str.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;
using mergerfs::Category;
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

    fs::erase_fnmatches(patterns,srcmounts);
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
_setxattr_policy(const Policy *policies[],
                 const string &attrname,
                 const string &attrval,
                 const int     flags)
{
  const Category *cat;
  const Policy   *policy;

  cat = Category::find(attrname);
  if(cat == Category::invalid)
    return -ENOATTR;

  if((flags & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  policy = Policy::find(attrval);
  if(policy == Policy::invalid)
    return -EINVAL;

  policies[*cat] = policy;

  return 0;
}

static
int
_setxattr_controlfile(config::Config &config,
                      const string   &attrname,
                      const string   &attrval,
                      const int       flags)
{
  vector<string>  nameparts;

  str::split(nameparts,attrname,'.');

  if(nameparts.size() != 3      ||
     nameparts[0]     != "user" ||
     nameparts[1]     != "mergerfs")
    return -ENOATTR;

  if(attrval.empty())
    return -EINVAL;

  if(nameparts[2] == "srcmounts")
    return _setxattr_srcmounts(config.srcmounts,
                               config.srcmountslock,
                               config.destmount,
                               attrval,
                               flags);

  return _setxattr_policy(config.policies,
                          nameparts[2],
                          attrval,
                          flags);
}

static
int
_setxattr(const fs::SearchFunc  searchFunc,
          const vector<string> &srcmounts,
          const string         &fusepath,
          const char           *attrname,
          const char           *attrval,
          const size_t          attrvalsize,
          const int             flags)
{
#ifndef WITHOUT_XATTR
  int rv;
  fs::Path path;

  rv = searchFunc(srcmounts,fusepath,path);
  if(rv == -1)
    return -errno;

  rv = ::lsetxattr(path.full.c_str(),attrname,attrval,attrvalsize,flags);

  return ((rv == -1) ? -errno : 0);
#else
  return -ENOTSUP;
#endif
}
namespace mergerfs
{
  namespace setxattr
  {
    int
    setxattr(const char *fusepath,
             const char *attrname,
             const char *attrval,
             size_t      attrvalsize,
             int         flags)
    {
      const config::Config &config = config::get();

      if(fusepath == config.controlfile)
        return _setxattr_controlfile(config::get_writable(),
                                     attrname,
                                     string(attrval,attrvalsize),
                                     flags);

      {
        const struct fuse_context *fc = fuse_get_context();
        const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
        const rwlock::ReadGuard    readlock(&config.srcmountslock);

        return _setxattr(*config.action,
                         config.srcmounts,
                         fusepath,
                         attrname,
                         attrval,
                         attrvalsize,
                         flags);
      }
    }
  }
}
