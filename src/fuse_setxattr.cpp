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
#include "fs_base_setxattr.hpp"
#include "fs_glob.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "num.hpp"
#include "rv.hpp"
#include "str.hpp"
#include "ugid.hpp"

#include <fuse.h>

#include <sstream>
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
  setxattr_controlfile(Config       &config,
                       const string &attrname,
                       const string &attrval,
                       const int     flags)
  {
    int rv;
    string key;

    if(!str::startswith(attrname,"user.mergerfs."))
      return -ENOATTR;

    key = &attrname[14];

    if(config.has_key(key) == false)
      return -ENOATTR;

    if((flags & XATTR_CREATE) == XATTR_CREATE)
      return -EEXIST;

    rv = config.set(key,attrval);
    if(rv < 0)
      return rv;

    config.open_cache.clear();
    fs::statvfs_cache_timeout(config.cache_statfs);

    return rv;
  }

  static
  int
  setxattr_loop_core(const string &basepath,
                     const char   *fusepath,
                     const char   *attrname,
                     const char   *attrval,
                     const size_t  attrvalsize,
                     const int     flags,
                     const int     error)
  {
    int rv;
    string fullpath;

    fullpath = fs::path::make(basepath,fusepath);

    rv = fs::lsetxattr(fullpath,attrname,attrval,attrvalsize,flags);

    return error::calc(rv,error,errno);
  }

  static
  int
  setxattr_loop(const vector<string> &basepaths,
                const char           *fusepath,
                const char           *attrname,
                const char           *attrval,
                const size_t          attrvalsize,
                const int             flags)
  {
    int error;

    error = -1;
    for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
      {
        error = l::setxattr_loop_core(basepaths[i],fusepath,
                                      attrname,attrval,attrvalsize,flags,
                                      error);
      }

    return -error;
  }

  static
  int
  setxattr(Policy::Func::Action  actionFunc,
           const Branches       &branches_,
           const uint64_t        minfreespace,
           const char           *fusepath,
           const char           *attrname,
           const char           *attrval,
           const size_t          attrvalsize,
           const int             flags)
  {
    int rv;
    vector<string> basepaths;

    rv = actionFunc(branches_,fusepath,minfreespace,&basepaths);
    if(rv == -1)
      return -errno;

    return l::setxattr_loop(basepaths,fusepath,attrname,attrval,attrvalsize,flags);
  }
}

namespace FUSE
{
  int
  setxattr(const char *fusepath,
           const char *attrname,
           const char *attrval,
           size_t      attrvalsize,
           int         flags)
  {
    Config &config = Config::rw();

    if(fusepath == config.controlfile)
      return l::setxattr_controlfile(config,
                                     attrname,
                                     string(attrval,attrvalsize),
                                     flags);

    if((config.security_capability == false) &&
       l::is_attrname_security_capability(attrname))
      return -ENOATTR;

    if(config.xattr.to_int())
      return -config.xattr.to_int();

    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::setxattr(config.func.setxattr.policy,
                       config.branches,
                       config.minfreespace,
                       fusepath,
                       attrname,
                       attrval,
                       attrvalsize,
                       flags);
  }
}
