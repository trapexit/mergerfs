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

#include "category.hpp"
#include "config.hpp"
#include "errno.hpp"
#include "fs_llistxattr.hpp"
#include "fs_path.hpp"
#include "ugid.hpp"
#include "xattr.hpp"

#include "fuse.h"

#include <string>

#include <string.h>

using std::string;


namespace l
{
  static
  int
  listxattr_controlfile(Config::Read &cfg_,
                        char         *list_,
                        const size_t  size_)
  {
    string xattrs;

    cfg_->keys_xattr(xattrs);
    if(size_ == 0)
      return xattrs.size();

    if(size_ < xattrs.size())
      return -ERANGE;

    memcpy(list_,xattrs.c_str(),xattrs.size());

    return xattrs.size();
  }

  static
  int
  listxattr(const Policy::Search &searchFunc_,
            const Branches       &branches_,
            const char           *fusepath_,
            char                 *list_,
            const size_t          size_)
  {
    int rv;
    string fullpath;
    StrVec basepaths;

    rv = searchFunc_(branches_,fusepath_,&basepaths);
    if(rv == -1)
      return -errno;

    fullpath = fs::path::make(basepaths[0],fusepath_);

    rv = fs::llistxattr(fullpath,list_,size_);

    return ((rv == -1) ? -errno : rv);
  }
}

namespace FUSE
{
  int
  listxattr(const char *fusepath_,
            char       *list_,
            size_t      size_)
  {
    Config::Read cfg;

    if(fusepath_ == CONTROLFILE)
      return l::listxattr_controlfile(cfg,list_,size_);

    switch(cfg->xattr)
      {
      case XAttr::ENUM::PASSTHROUGH:
        break;
      case XAttr::ENUM::NOATTR:
        return 0;
      case XAttr::ENUM::NOSYS:
        return -ENOSYS;
      }

    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::listxattr(cfg->func.listxattr.policy,
                        cfg->branches,
                        fusepath_,
                        list_,
                        size_);
  }
}
