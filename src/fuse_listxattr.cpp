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

#include <filesystem>
#include <optional>
#include <string>
#include <cstring>


static
ssize_t
_listxattr_size(const std::vector<Branch*> &branches_,
                const fs::path             &fusepath_)
{
  ssize_t size;
  fs::path fullpath;

  if(branches_.empty())
    return -ENOENT;

  size = 0;
  for(const auto branch : branches_)
    {
      ssize_t rv;

      fullpath = branch->path / fusepath_;

      rv = fs::llistxattr(fullpath,NULL,0);
      if(rv < 0)
        continue;

      size += rv;
    }

  return size;
}

static
ssize_t
_listxattr(const std::vector<Branch*> &branches_,
           const fs::path             &fusepath_,
           char                       *list_,
           size_t                      size_)
{
  ssize_t rv;
  ssize_t size;
  fs::path fullpath;
  std::optional<ssize_t> err;

  if(size_ == 0)
    return ::_listxattr_size(branches_,fusepath_);

  size = 0;
  err  = -ENOENT;
  for(const auto branch : branches_)
    {
      fullpath = branch->path / fusepath_;

      rv = fs::llistxattr(fullpath,list_,size_);
      if(rv == -ERANGE)
        return -ERANGE;
      if(rv < 0)
        {
          if(!err.has_value())
            err = rv;
          continue;
        }

      err = 0;
      list_ += rv;
      size_ -= rv;
      size  += rv;
    }

  if(err < 0)
    return err.value();

  return size;
}

static
int
_listxattr(const Policy::Search &searchFunc_,
           const Branches       &ibranches_,
           const fs::path       &fusepath_,
           char                 *list_,
           const size_t          size_)
{
  int rv;
  std::vector<Branch*> obranches;

  rv = searchFunc_(ibranches_,fusepath_,obranches);
  if(rv < 0)
    return rv;

  if(size_ == 0)
    return ::_listxattr_size(obranches,fusepath_);

  return ::_listxattr(obranches,fusepath_,list_,size_);
}

int
FUSE::listxattr(const fuse_req_ctx_t *ctx_,
                const char           *fusepath_,
                char                 *list_,
                size_t                size_)
{
  const fs::path fusepath{fusepath_};

  if(Config::is_ctrl_file(fusepath))
    return cfg.keys_listxattr(list_,size_);

  switch(cfg.xattr)
    {
    case XAttr::ENUM::PASSTHROUGH:
      break;
    case XAttr::ENUM::NOATTR:
      return 0;
    case XAttr::ENUM::NOSYS:
      return -ENOSYS;
    }

  return ::_listxattr(cfg.func.listxattr.policy,
                      cfg.branches,
                      fusepath,
                      list_,
                      size_);
}
