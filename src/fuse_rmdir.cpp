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

#include "fuse_rmdir.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "error.hpp"
#include "fs_path.hpp"
#include "fs_rmdir.hpp"
#include "fs_unlink.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>

#include <unistd.h>


static
int
_should_unlink(int            rv_,
               FollowSymlinks followsymlinks_)
{
  return ((rv_ == -ENOTDIR) &&
          (followsymlinks_ != FollowSymlinks::ENUM::NEVER));
}

static
int
_rmdir_core(const fs::path       &basepath_,
            const fs::path       &fusepath_,
            const FollowSymlinks  followsymlinks_)
{
  int rv;
  fs::path fullpath;

  fullpath = basepath_ / fusepath_;

  rv = fs::rmdir(fullpath);
  if(::_should_unlink(rv,followsymlinks_))
    rv = fs::unlink(fullpath);

  return rv;
}

static
int
_rmdir_loop(const std::vector<Branch*> &branches_,
            const fs::path             &fusepath_,
            const FollowSymlinks        followsymlinks_)
{
  Err err;

  for(const auto &branch : branches_)
    {
      err = ::_rmdir_core(branch->path,fusepath_,followsymlinks_);
    }

  return err;
}

static
int
_rmdir(const Policy::Action &actionFunc_,
       const Branches       &branches_,
       const FollowSymlinks  followsymlinks_,
       const fs::path       &fusepath_)
{
  int rv;
  std::vector<Branch*> branches;

  rv = actionFunc_(branches_,fusepath_,branches);
  if(rv < 0)
    return rv;

  return ::_rmdir_loop(branches,fusepath_,followsymlinks_);
}

int
FUSE::rmdir(const fuse_req_ctx_t *ctx_,
            const char           *fusepath_)
{
  const fs::path  fusepath{fusepath_};
  const ugid::Set ugid(ctx_);

  return ::_rmdir(cfg.func.rmdir.policy,
                  cfg.branches,
                  cfg.follow_symlinks,
                  fusepath);
}
