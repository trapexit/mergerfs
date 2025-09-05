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

#include <optional>
#include <string>

#include <cstring>

static
ssize_t
_listxattr_size(const std::vector<Branch*> &branches_,
                const char                 *fusepath_)
{
  ssize_t size;
  std::string fullpath;

  if(branches_.empty())
    return -ENOENT;

  size = 0;
  for(const auto branch : branches_)
    {
      ssize_t rv;

      fullpath = fs::path::make(branch->path,fusepath_);

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
           const char                 *fusepath_,
           char                       *list_,
           size_t                      size_)
{
  ssize_t rv;
  ssize_t size;
  std::string fullpath;
  std::optional<ssize_t> err;

  if(size_ == 0)
    return ::_listxattr_size(branches_,fusepath_);

  size = 0;
  err  = -ENOENT;
  for(const auto branch : branches_)
    {
      fullpath = fs::path::make(branch->path,fusepath_);

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
           const char           *fusepath_,
           char                 *list_,
           const size_t          size_)
{
  int rv;
  std::string fullpath;
  std::vector<Branch*> obranches;

  rv = searchFunc_(ibranches_,fusepath_,obranches);
  if(rv < 0)
    return rv;

  if(size_ == 0)
    return ::_listxattr_size(obranches,fusepath_);

  return ::_listxattr(obranches,fusepath_,list_,size_);
}

int
FUSE::listxattr(const char *fusepath_,
                char       *list_,
                size_t      size_)
{
  if(Config::is_ctrl_file(fusepath_))
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

  const fuse_context *fc = fuse_get_context();
  const ugid::Set     ugid(fc->uid,fc->gid);

  return ::_listxattr(cfg.func.listxattr.policy,
                      cfg.branches,
                      fusepath_,
                      list_,
                      size_);
}
