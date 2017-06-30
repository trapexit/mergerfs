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

#include <string.h>

#include "buildvector.hpp"
#include "category.hpp"
#include "config.hpp"
#include "errno.hpp"
#include "fs_base_listxattr.hpp"
#include "fs_path.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"
#include "xattr.hpp"

using std::string;
using std::vector;
using namespace mergerfs;

static
int
_listxattr_controlfile(char         *list,
                       const size_t  size)
{
  string xattrs;
  const vector<string> strs =
    buildvector<string>
    ("user.mergerfs.srcmounts")
    ("user.mergerfs.minfreespace")
    ("user.mergerfs.moveonenospc")
    ("user.mergerfs.dropcacheonclose")
    ("user.mergerfs.symlinkify")
    ("user.mergerfs.symlinkify_timeout")
    ("user.mergerfs.nullrw")
    ("user.mergerfs.ignorepponrename")
    ("user.mergerfs.policies")
    ("user.mergerfs.version")
    ("user.mergerfs.pid");

  xattrs.reserve(1024);
  for(size_t i = 0; i < strs.size(); i++)
    xattrs += (strs[i] + '\0');
  for(size_t i = Category::Enum::BEGIN; i < Category::Enum::END; i++)
    xattrs += ("user.mergerfs.category." + (std::string)*Category::categories[i] + '\0');
  for(size_t i = FuseFunc::Enum::BEGIN; i < FuseFunc::Enum::END; i++)
    xattrs += ("user.mergerfs.func." + (std::string)*FuseFunc::fusefuncs[i] + '\0');

  if(size == 0)
    return xattrs.size();

  if(size < xattrs.size())
    return -ERANGE;

  memcpy(list,xattrs.c_str(),xattrs.size());

  return xattrs.size();
}

static
int
_listxattr(Policy::Func::Search  searchFunc,
           const vector<string> &srcmounts,
           const uint64_t        minfreespace,
           const char           *fusepath,
           char                 *list,
           const size_t          size)
{
  int rv;
  string fullpath;
  vector<const string*> basepaths;

  rv = searchFunc(srcmounts,fusepath,minfreespace,basepaths);
  if(rv == -1)
    return -errno;

  fs::path::make(basepaths[0],fusepath,fullpath);

  rv = fs::llistxattr(fullpath,list,size);

  return ((rv == -1) ? -errno : rv);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    listxattr(const char *fusepath,
              char       *list,
              size_t      size)
    {
      const fuse_context *fc     = fuse_get_context();
      const Config       &config = Config::get(fc);

      if(fusepath == config.controlfile)
        return _listxattr_controlfile(list,size);

      const ugid::Set         ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard readlock(&config.srcmountslock);

      return _listxattr(config.listxattr,
                        config.srcmounts,
                        config.minfreespace,
                        fusepath,
                        list,
                        size);
    }
  }
}
