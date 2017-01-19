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

#include "config.hpp"
#include "errno.hpp"
#include "fs_base_removexattr.hpp"
#include "fs_path.hpp"
#include "rv.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;

static
int
_removexattr_loop_core(const string *basepath,
                       const char   *fusepath,
                       const char   *attrname,
                       const int     error)
{
  int rv;
  string fullpath;

  fs::path::make(basepath,fusepath,fullpath);

  rv = fs::lremovexattr(fullpath,attrname);

  return error::calc(rv,error,errno);
}

static
int
_removexattr_loop(const vector<const string*> &basepaths,
                  const char                  *fusepath,
                  const char                  *attrname)
{
  int error;

  error = -1;

  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      error = _removexattr_loop_core(basepaths[i],fusepath,attrname,error);
    }

  return -error;
}

static
int
_removexattr(Policy::Func::Action  actionFunc,
             const vector<string> &srcmounts,
             const uint64_t        minfreespace,
             const char           *fusepath,
             const char           *attrname)
{
  int rv;
  vector<const string*> basepaths;

  rv = actionFunc(srcmounts,fusepath,minfreespace,basepaths);
  if(rv == -1)
    return -errno;

  return _removexattr_loop(basepaths,fusepath,attrname);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    removexattr(const char *fusepath,
                const char *attrname)
    {
      const fuse_context *fc     = fuse_get_context();
      const Config       &config = Config::get(fc);

      if(fusepath == config.controlfile)
        return -ENOTSUP;

      const ugid::Set         ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard readlock(&config.srcmountslock);

      return _removexattr(config.removexattr,
                          config.srcmounts,
                          config.minfreespace,
                          fusepath,
                          attrname);
    }
  }
}
