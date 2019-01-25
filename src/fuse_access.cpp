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

#include "config.hpp"
#include "errno.hpp"
#include "fs_base_access.hpp"
#include "fs_path.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;

namespace l
{
  static
  int
  access(Policy::Func::Search  searchFunc,
         const Branches       &branches_,
         const uint64_t        minfreespace,
         const char           *fusepath,
         const int             mask)
  {
    int rv;
    string fullpath;
    vector<const string*> basepaths;

    rv = searchFunc(branches_,fusepath,minfreespace,basepaths);
    if(rv == -1)
      return -errno;

    fullpath = fs::path::make(basepaths[0],fusepath);

    rv = fs::eaccess(fullpath,mask);

    return ((rv == -1) ? -errno : 0);
  }
}

namespace FUSE
{
  int
  access(const char *fusepath,
         int         mask)
  {
    const fuse_context      *fc     = fuse_get_context();
    const Config            &config = Config::get(fc);
    const ugid::Set          ugid(fc->uid,fc->gid);
    const rwlock::ReadGuard  readlock(&config.branches_lock);

    return l::access(config.access,
                     config.branches,
                     config.minfreespace,
                     fusepath,
                     mask);
  }
}
