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

#include "config.hpp"
#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_link.hpp"
#include "fs_lstat.hpp"
#include "fs_path.hpp"
#include "fuse_getattr.hpp"
#include "fuse_symlink.hpp"

#include "fuse.h"

#include <optional>
#include <string>
#include <vector>


static
int
_link_create_path_loop(const std::vector<Branch*> &oldbranches_,
                       const Branch               *newbranch_,
                       const fs::relpath             &oldfusepath_,
                       const fs::relpath             &newfusepath_,
                       const fs::relpath             &newfusedirpath_)
{
  int rv;
  int err;
  fs::relpath oldfullpath = oldfusepath_;
  fs::relpath newfullpath = newfusepath_;

  err = -ENOENT;
  for(const auto &oldbranch : oldbranches_)
    {
      oldfullpath.set_prefix(oldbranch->path);
      newfullpath.set_prefix(oldbranch->path);

      rv = fs::link(oldfullpath,newfullpath);
      if(rv == -ENOENT)
        {
          rv = fs::clonepath(newbranch_->path,
                             oldbranch->path,
                             newfusedirpath_);
          if(rv == 0)
            rv = fs::link(oldfullpath,newfullpath);
        }

      if(err < 0)
        err = rv;
    }

  return err;
}

static
int
_link_create_path(const Policy::Search &searchFunc_,
                  const Policy::Action &actionFunc_,
                  const Branches::Ptr   ibranches_,
                  const fs::relpath       &oldfusepath_,
                  const fs::relpath       &newfusepath_)
{
  int rv;
  fs::relpath newfusedirpath;
  std::vector<Branch*> oldbranches;
  std::vector<Branch*> newbranches;

  rv = actionFunc_(ibranches_,oldfusepath_,oldbranches);
  if(rv < 0)
    return rv;
  if(oldbranches.empty())
    return -ENOENT;

  newfusedirpath = newfusepath_.parent_path();

  rv = searchFunc_(ibranches_,newfusedirpath,newbranches);
  if(rv < 0)
    return rv;
  if(newbranches.empty())
    return -ENOENT;

  return ::_link_create_path_loop(oldbranches,newbranches[0],
                                  oldfusepath_,newfusepath_,
                                  newfusedirpath);
}

static
int
_link_preserve_path_loop(const std::vector<Branch*> &oldbranches_,
                         const fs::relpath             &oldfusepath_,
                         const fs::relpath             &newfusepath_,
                         struct stat                *st_)
{
  int rv;
  int err;
  fs::relpath oldfullpath = oldfusepath_;
  fs::relpath newfullpath = newfusepath_;

  err = -ENOENT;
  for(const auto &oldbranch : oldbranches_)
    {
      oldfullpath.set_prefix(oldbranch->path);
      newfullpath.set_prefix(oldbranch->path);

      rv = fs::link(oldfullpath,newfullpath);
      if(rv == -ENOENT)
        rv = -EXDEV;
      if((rv == 0) && (st_->st_ino == 0))
        rv = fs::lstat(oldfullpath,st_);

      if(err < 0)
        err = rv;
    }

  return err;
}

static
int
_link_preserve_path(const Policy::Action &actionFunc_,
                    const Branches::Ptr   branches_,
                    const fs::relpath       &oldfusepath_,
                    const fs::relpath       &newfusepath_,
                    struct stat          *st_)
{
  int rv;
  std::vector<Branch*> oldbranches;

  rv = actionFunc_(branches_,oldfusepath_,oldbranches);
  if(rv < 0)
    return rv;
  if(oldbranches.empty())
    return -ENOENT;

  return ::_link_preserve_path_loop(oldbranches,
                                    oldfusepath_,
                                    newfusepath_,
                                    st_);
}

static
int
_link(const fs::relpath &oldpath_,
      const fs::relpath &newpath_,
      struct stat    *st_)
{
  if(cfg.func.create.policy.path_preserving() && !cfg.ignorepponrename)
    return ::_link_preserve_path(cfg.func.link.policy,
                                 cfg.branches,
                                 oldpath_,
                                 newpath_,
                                 st_);

  return ::_link_create_path(cfg.func.getattr.policy,
                             cfg.func.link.policy,
                             cfg.branches,
                             oldpath_,
                             newpath_);
}

static
int
_link(const fuse_req_ctx_t *ctx_,
      const fs::relpath       &oldpath_,
      const fs::relpath       &newpath_,
      struct stat          *st_,
      fuse_timeouts_t      *timeouts_)
{
  int rv;

  rv = ::_link(oldpath_,newpath_,st_);
  if(rv < 0)
    return rv;

  return FUSE::getattr(newpath_,st_,timeouts_);
}

static
int
_link_exdev_rel_symlink(const fuse_req_ctx_t *ctx_,
                        const fs::relpath       &oldpath_,
                        const fs::relpath       &newpath_,
                        struct stat          *st_,
                        fuse_timeouts_t      *timeouts_)
{
  int rv;
  // lexically_relative builds the target as a relpath; newpath_ is
  // passed straight through to FUSE::symlink without copying.
  fs::relpath target = oldpath_.lexically_relative(newpath_.parent_path());
  if(target.empty())
    return -EXDEV;

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
_link_exdev_abs_base_symlink(const fuse_req_ctx_t *ctx_,
                              const Policy::Search &openPolicy_,
                              const Branches::Ptr   ibranches_,
                             const fs::relpath       &oldpath_,
                             const fs::relpath       &newpath_,
                             struct stat          *st_,
                             fuse_timeouts_t      *timeouts_)
{
  int rv;
  fs::relpath target;
  std::vector<Branch*> obranches;

  rv = openPolicy_(ibranches_,oldpath_,obranches);
  if(rv < 0)
    return rv;
  if(obranches.empty())
    return -ENOENT;

  target = obranches[0]->path / oldpath_;

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
                             const std::string    &mount_,
                             const fs::relpath    &oldpath_,
                             const fs::relpath    &newpath_,
                             struct stat          *st_,
                             fuse_timeouts_t      *timeouts_)
{
  int rv;
  StrVec basepaths;
  fs::relpath target;

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
            const fs::relpath       &oldpath_,
            const fs::relpath       &newpath_,
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
                                            cfg.func.open.policy,
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
           const fs::relpath       &oldpath_,
           const fs::relpath       &newpath_,
           struct stat          *st_,
           fuse_timeouts_t      *timeouts_)
{
  int rv;

  rv = ::_link(ctx_,oldpath_,newpath_,st_,timeouts_);
  if(rv == -EXDEV)
    rv = ::_link_exdev(ctx_,oldpath_,newpath_,st_,timeouts_);

  return rv;
}
