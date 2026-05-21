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

#include "fuse_rename.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_exists.hpp"
#include "fs_link.hpp"
#include "fs_mkdir_as.hpp"
#include "fs_path.hpp"
#include "fs_remove.hpp"
#include "fs_rename.hpp"
#include "fs_symlink.hpp"
#include "fs_unlink.hpp"
#include "fuse_symlink.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>


static
void
_rename_exdev_rename_back(const std::vector<Branch*> &branches_,
                          const fs::path             &oldfusepath_)
{
  fs::path oldpath;
  fs::path newpath;

  for(auto &branch : branches_)
    {
      oldpath  = branch->path;
      oldpath /= ".mergerfs_rename_exdev";
      oldpath /= oldfusepath_;

      newpath  = branch->path;
      newpath /= oldfusepath_;

      fs::rename(oldpath,newpath);
    }
}

static
int
_rename_exdev_rename_target(const Branches::Ptr  &ibranches_,
                            const fs::path       &oldfusepath_,
                            std::vector<Branch*> &obranches_)
{
  int rv;
  fs::path clonesrc;
  fs::path clonedst;

  for(auto &branch : *ibranches_)
    {
      if(branch.ro())
        continue;
      if(!fs::exists(branch.path,oldfusepath_))
        continue;
      obranches_.emplace_back(&branch);
    }
  if(obranches_.empty())
    return -ENOENT;

  for(auto &branch : obranches_)
    {
      clonesrc  = branch->path;
      clonedst  = branch->path;
      clonedst /= ".mergerfs_rename_exdev";

      rv = fs::clonepath(clonesrc,clonedst,oldfusepath_.parent_path());
      if(rv == -ENOENT)
        {
          fs::mkdir_as({0,0},clonedst,01777);
          rv = fs::clonepath(clonesrc,clonedst,oldfusepath_.parent_path());
        }

      if(rv < 0)
        goto error;

      clonesrc /= oldfusepath_;
      clonedst /= oldfusepath_;

      rv = fs::rename(clonesrc,clonedst);
      if(rv < 0)
        goto error;
    }

  return 0;

 error:
  ::_rename_exdev_rename_back(obranches_,oldfusepath_);

  return -EXDEV;
}

static
int
_rename_exdev_rel_symlink(const fuse_req_ctx_t *ctx_,
                          const Branches::Ptr  &branches_,
                          const fs::path       &oldfusepath_,
                          const fs::path       &newfusepath_)
{
  int rv;
  fs::path target;
  fs::path linkpath;
  std::vector<Branch*> branches;

  rv = ::_rename_exdev_rename_target(branches_,
                                     oldfusepath_,
                                     branches);
  if(rv < 0)
    return rv;

  linkpath  = newfusepath_;
  target    = "/.mergerfs_rename_exdev";
  target   /= oldfusepath_;
  target    = target.lexically_relative(linkpath.parent_path());

  rv = FUSE::symlink(ctx_,target.c_str(),linkpath);
  if(rv < 0)
    ::_rename_exdev_rename_back(branches,oldfusepath_);

  return rv;
}

static
int
_rename_exdev_abs_symlink(const fuse_req_ctx_t *ctx_,
                          const Branches::Ptr  &branches_,
                          const fs::path       &mount_,
                          const fs::path       &oldfusepath_,
                          const fs::path       &newfusepath_)
{
  int rv;
  fs::path target;
  fs::path linkpath;
  std::vector<Branch*> branches;

  rv = ::_rename_exdev_rename_target(branches_,
                                     oldfusepath_,
                                     branches);
  if(rv < 0)
    return rv;

  linkpath  = newfusepath_;
  target    = mount_;
  target   /= ".mergerfs_rename_exdev";
  target   /= oldfusepath_;

  rv = FUSE::symlink(ctx_,target.c_str(),linkpath);
  if(rv < 0)
    ::_rename_exdev_rename_back(branches,oldfusepath_);

  return rv;
}

static
int
_rename_exdev(const fuse_req_ctx_t *ctx_,
              const fs::path       &oldfusepath_,
              const fs::path       &newfusepath_)
{
  switch(cfg.rename_exdev)
    {
    case RenameEXDEV::ENUM::PASSTHROUGH:
      return -EXDEV;
    case RenameEXDEV::ENUM::REL_SYMLINK:
      return ::_rename_exdev_rel_symlink(ctx_,
                                         cfg.branches,
                                         oldfusepath_,
                                         newfusepath_);
    case RenameEXDEV::ENUM::ABS_SYMLINK:
      return ::_rename_exdev_abs_symlink(ctx_,
                                         cfg.branches,
                                         cfg.mountpoint,
                                         oldfusepath_,
                                         newfusepath_);
    }

  return -EXDEV;
}

static
bool
_create_is_path_preserving()
{
  auto impl = cfg.create.impl();
  return impl && impl->path_preserving();
}

static
int
_rename(const fs::path &oldpath_,
        const fs::path &newpath_)
{
  // Honor ignorepponrename: when the create policy is path-preserving
  // and the user has not opted out, refuse to clone the new path
  // structure into branches that don't already have it. The fallback
  // is the EXDEV path which lets the caller handle the move via
  // copy+unlink or symlink per cfg.rename_exdev.
  if(!cfg.ignorepponrename && ::_create_is_path_preserving())
    {
      // Snapshot the Branches::Impl so the per-branch fs::exists
      // calls below all observe the same set of branches even if
      // cfg.branches is concurrently mutated.
      Branches::Ptr branches = cfg.branches;
      const fs::path newdir = newpath_.parent_path();
      bool any_target = false;
      for(const auto &branch : *branches)
        {
          if(branch.ro())
            continue;
          if(fs::exists(branch.path,oldpath_) &&
             fs::exists(branch.path,newdir))
            { any_target = true; break; }
        }
      if(!any_target)
        return -EXDEV;
    }

  return cfg.rename(cfg.branches,oldpath_,newpath_);
}

int
FUSE::rename(const fuse_req_ctx_t *ctx_,
             const char           *oldfusepath_,
             const char           *newfusepath_)
{
  int rv;
  const fs::path oldfusepath{oldfusepath_};
  const fs::path newfusepath{newfusepath_};

  rv = ::_rename(oldfusepath,newfusepath);
  if(rv == -EXDEV)
    return ::_rename_exdev(ctx_,oldfusepath,newfusepath);

  return rv;
}
