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

#include <string>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "config.hpp"
#include "fs_path.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;
using mergerfs::Category;

static
int
_access(Policy::Func::Search  searchFunc,
        const vector<string> &srcmounts,
        const uint64_t        minfreespace,
        const char           *fusepath,
        const int             mask)
{
  int rv;
  string fullpath;
  vector<const string*> basepaths;

  rv = searchFunc(srcmounts,fusepath,minfreespace,basepaths);
  if(rv == -1)
    return -errno;

  fs::path::make(basepaths[0],fusepath,fullpath);

  rv = ::faccessat(AT_FDCWD,fullpath.c_str(),mask,AT_EACCESS);

  return ((rv == -1) ? -errno : 0);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    access(const char *fusepath,
           int         mask)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.srcmountslock);

      return _access(config.access,
                     config.srcmounts,
                     config.minfreespace,
                     fusepath,
                     mask);
    }
  }
}
