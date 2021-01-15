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

#include "config.hpp"
#include "errno.hpp"
#include "fs_eaccess.hpp"
#include "fs_path.hpp"
#include "ugid.hpp"

#include <string>
#include <vector>

using std::string;
using std::vector;


namespace l
{
  static
  int
  access(const Policy::Search &searchFunc_,
         const Branches       &branches_,
         const char           *fusepath_,
         const int             mask_)
  {
    int rv;
    string fullpath;
    StrVec basepaths;

    rv = searchFunc_(branches_,fusepath_,&basepaths);
    if(rv == -1)
      return -errno;

    fullpath = fs::path::make(basepaths[0],fusepath_);

    rv = fs::eaccess(fullpath,mask_);

    return ((rv == -1) ? -errno : 0);
  }
}

namespace FUSE
{
  int
  access(const char *fusepath_,
         int         mask_)
  {
    Config::Read cfg;
    const fuse_context *fc  = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::access(cfg->func.access.policy,
                     cfg->branches,
                     fusepath_,
                     mask_);
  }
}
