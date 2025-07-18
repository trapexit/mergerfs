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

#include "fuse_listxattr.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fs_llistxattr.hpp"
#include "ugid.hpp"
#include "xattr.hpp"

#include "fuse.h"

#include <string>

#include <cstring>

static
ssize_t
_listxattr_size(const std::vector<Branch*>)
{
  ssize_t rv;
  ssize_t size;
  std::string fullpath;
  Branches::Ptr branches;

  size = 0;
  branches = cfg_->branches;
  for(const auto &branch : *branches)
    {
      rv = fs::llistxattr(branch.path,NULL,0);
      if(rv < 0)
        continue;

      size += rv;
    }

  return rv;
}

static
ssize_t
_root_listxattr(Config::Read &cfg_,
                char         *list_,
                size_t        size_)
{
  if(size_ == 0)
    return ::_root_listxattr_size(cfg_);

  ssize_t rv;
  ssize_t size = 0;
  Branches::Ptr branches = cfg_->branches;
  for(const auto &branch : *branches)
    {
      rv = fs::llistxattr(branch.path,list_,size_);
      if(rv == -ERANGE)
        return -ERANGE;
      if(rv < 0)
        continue;

      list_ += rv;
      size_ -= rv;
      size  += rv;
    }

  return size;
}

static
int
_listxattr(const Policy::Search &searchFunc_,
           const Branches       &ibranches_,
           const char           *fusepath_,
           char                 *list_,
           const size_t          size_)
{
  int rv;
  std::string fullpath;
  std::vector<Branch*> obranches;

  rv = searchFunc_(ibranches_,fusepath_,obranches);
  if(rv == -1)
    return -errno;

  fullpath = fs::path::make(obranches[0]->path,fusepath_);

  rv = fs::llistxattr(fullpath,list_,size_);

  return ((rv == -1) ? -errno : rv);
}

int
FUSE::listxattr(const char *fusepath_,
                char       *list_,
                size_t      size_)
{
  Config::Read cfg;

  if(Config::is_ctrl_file(fusepath_))
    return cfg->keys_listxattr(list_,size_);

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

  return ::_listxattr(cfg->func.listxattr.policy,
                      cfg->branches,
                      fusepath_,
                      list_,
                      size_);
}
