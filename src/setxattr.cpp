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
#include "assert.hpp"
#include "xattr.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;
using mergerfs::Category;
using namespace mergerfs;

template<typename Container>
Container&
split(Container                                  &result,
      const typename Container::value_type       &s,
      typename Container::value_type::value_type  delimiter)
{
  std::string        str;
  std::istringstream ss(s);

  while(std::getline(ss,str,delimiter))
    result.push_back(str);

  return result;
}

static
int
_setxattr_controlfile(config::Config &config,
                      const string    attrname,
                      const string    attrval,
                      const size_t    attrvalsize,
                      const int       flags)
{
#ifndef WITHOUT_XATTR
  const Category *cat;
  const Policy   *policy;
  vector<string>  nameparts;

  split(nameparts,attrname,'.');

  if(nameparts.size() != 3)
    return -EINVAL;
  
  if(nameparts[0] != "user")
    return -ENOATTR;
  
  if(nameparts[1] != "mergerfs")
    return -ENOATTR;

  cat = Category::find(nameparts[2]);
  if(cat == Category::invalid)
    return -ENOATTR;

  if((flags & XATTR_CREATE) == XATTR_CREATE)
    return -EEXIST;

  policy = Policy::find(attrval);
  if(policy == Policy::invalid)
    return -EINVAL;

  config.policies[*cat] = policy;

  return 0;
#else
  return -ENOTSUP;
#endif
}

static
int
_setxattr(const fs::SearchFunc  searchFunc,
          const vector<string> &srcmounts,
          const string          fusepath,
          const char           *attrname,
          const char           *attrval,
          const size_t          attrvalsize,
          const int             flags)
{
#ifndef WITHOUT_XATTR
  int rv;
  int error;
  fs::PathVector paths;

  searchFunc(srcmounts,fusepath,paths);
  if(paths.empty())
    return -ENOENT;

  rv    = -1;
  error =  0;
  for(fs::PathVector::const_iterator
        i = paths.begin(), ei = paths.end(); i != ei; ++i)
    {
      rv &= ::lsetxattr(i->full.c_str(),attrname,attrval,attrvalsize,flags);
      if(rv == -1)
        error = errno;
    }

  return ((rv == -1) ? -error : 0);
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
      const struct fuse_context *fc     = fuse_get_context();
      const config::Config      &config = config::get();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);

      if(fusepath == config.controlfile)
        return _setxattr_controlfile(config::get_writable(),
                                     attrname,
                                     string(attrval,attrvalsize),
                                     attrvalsize,
                                     flags);

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
