/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_link.hpp"

#include "clone_source.hpp"
#include "config.hpp"
#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_exists.hpp"
#include "fs_link.hpp"
#include "fs_lstat.hpp"
#include "fs_path.hpp"
#include "fuse_getattr.hpp"
#include "fuse_symlink.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <optional>
#include <string>
#include <vector>


static
bool
_create_is_path_preserving()
{
  auto impl = cfg.create.impl();
  return impl && impl->path_preserving();
}

// Path-preserving link: only link on branches that already have BOTH oldpath
// and newdir; never clone newdir into branches that lacked it.
static
int
_link_preserve_path(const fs::path &oldpath_,
                    const fs::path &newpath_)
{
  Branches::Ptr branches = cfg.branches;
  const fs::path newdir = newpath_.parent_path();
  int err;
  bool found;
  fs::path oldfullpath;
  fs::path newfullpath;

  err   = 0;
  found = false;
  for(auto &branch : *branches)
    {
      if(branch.ro())
        continue;
      if(!fs::exists(branch.path,oldpath_))
        continue;
      if(!fs::exists(branch.path,newdir))
        continue;

      oldfullpath = branch.path / oldpath_;
      newfullpath = branch.path / newpath_;

      const int rv = fs::link(oldfullpath,newfullpath);

      if(!found)
        { err = rv; found = true; continue; }
      if(rv == 0)
        { err = 0; continue; }
      if(err == 0)
        continue;
      err = rv;
    }

  if(!found)
    return -EXDEV;

  return err;
}

static
int
_link(const fuse_req_ctx_t *ctx_,
      const fs::path       &oldpath_,
      const fs::path       &newpath_,
      struct stat          *st_,
      fuse_timeouts_t      *timeouts_)
{
  int rv;

  // Honor path-preserving create policies: link only on branches that
  // already have both old and new-parent path; never clone the new path
  // structure into branches that don't already have it. Fall through
  // to the link_exdev handling (PASSTHROUGH/REL_SYMLINK/etc.) when no
  // qualifying branch exists.
  if(!cfg.ignorepponrename && ::_create_is_path_preserving())
    rv = ::_link_preserve_path(oldpath_,newpath_);
  else
    rv = cfg.link(cfg.branches,oldpath_,newpath_);

  if(rv < 0)
    return rv;

  return FUSE::getattr(newpath_,st_,timeouts_);
}

static
int
_link_exdev_rel_symlink(const fuse_req_ctx_t *ctx_,
                        const fs::path       &oldpath_,
                        const fs::path       &newpath_,
                        struct stat          *st_,
                        fuse_timeouts_t      *timeouts_)
{
  int rv;
  fs::path target(oldpath_);
  fs::path linkpath(newpath_);

  target = target.lexically_relative(linkpath.parent_path());

  rv = FUSE::symlink(ctx_,target.c_str(),linkpath);
  if(rv == 0)
    rv = FUSE::getattr(ctx_,oldpath_,st_,timeouts_);

  // Disable caching since we created a symlink but should be a regular.
  timeouts_->attr  = 0;
  timeouts_->entry = 0;

  return rv;
}

static
int
_link_exdev_abs_base_symlink(const fuse_req_ctx_t *ctx_,
                             const Branches       &branches_,
                             const fs::path       &oldpath_,
                             const fs::path       &newpath_,
                             struct stat          *st_,
                             fuse_timeouts_t      *timeouts_)
{
  int rv;
  fs::path target;

  // Honor func.open: pick the source branch via the configured open
  // policy. For 'newest' this picks the branch holding the newest copy
  // of oldpath; for the other defaults this is first-found.
  Branches::Ptr branches = branches_;
  const Branch *src = CloneSource::find(*branches,oldpath_,cfg.open.to_string());
  if(src == nullptr)
    return -ENOENT;

  target = src->path / oldpath_;

  rv = FUSE::symlink(ctx_,target.c_str(),newpath_);
  if(rv == 0)
    rv = FUSE::getattr(ctx_,oldpath_,st_,timeouts_);

  // Disable caching since we created a symlink but should be a regular.
  timeouts_->attr  = 0;
  timeouts_->entry = 0;

  return rv;
}

static
int
_link_exdev_abs_pool_symlink(const fuse_req_ctx_t *ctx_,
                             const fs::path       &mount_,
                             const fs::path       &oldpath_,
                             const fs::path       &newpath_,
                             struct stat          *st_,
                             fuse_timeouts_t      *timeouts_)
{
  int rv;
  fs::path target;

  target = mount_ / oldpath_;

  rv = FUSE::symlink(ctx_,target.c_str(),newpath_);
  if(rv == 0)
    rv = FUSE::getattr(ctx_,oldpath_,st_,timeouts_);

  // Disable caching since we created a symlink but should be a regular.
  timeouts_->attr  = 0;
  timeouts_->entry = 0;

  return rv;
}

static
int
_link_exdev(const fuse_req_ctx_t *ctx_,
            const fs::path       &oldpath_,
            const fs::path       &newpath_,
            struct stat          *st_,
            fuse_timeouts_t      *timeouts_)
{
  switch(cfg.link_exdev)
    {
    case LinkEXDEV::ENUM::PASSTHROUGH:
      return -EXDEV;
    case LinkEXDEV::ENUM::REL_SYMLINK:
      return ::_link_exdev_rel_symlink(ctx_,
                                       oldpath_,
                                       newpath_,
                                       st_,
                                       timeouts_);
    case LinkEXDEV::ENUM::ABS_BASE_SYMLINK:
      return ::_link_exdev_abs_base_symlink(ctx_,
                                            cfg.branches,
                                            oldpath_,
                                            newpath_,
                                            st_,
                                            timeouts_);
    case LinkEXDEV::ENUM::ABS_POOL_SYMLINK:
      return ::_link_exdev_abs_pool_symlink(ctx_,
                                            cfg.mountpoint,
                                            oldpath_,
                                            newpath_,
                                            st_,
                                            timeouts_);
    }

  return -EXDEV;
}

int
FUSE::link(const fuse_req_ctx_t *ctx_,
           const char           *oldpath_,
           const char           *newpath_,
           struct stat          *st_,
           fuse_timeouts_t      *timeouts_)
{
  int rv;
  const fs::path oldpath{oldpath_};
  const fs::path newpath{newpath_};

  rv = ::_link(ctx_,oldpath,newpath,st_,timeouts_);
  if(rv == -EXDEV)
    rv = ::_link_exdev(ctx_,oldpath,newpath,st_,timeouts_);

  return rv;
}
