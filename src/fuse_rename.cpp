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

#include "fuse_rename.hpp"

#include "config.hpp"
#include "error.hpp"
#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_link.hpp"
#include "fs_mkdir_as.hpp"
#include "fs_path.hpp"
#include "fs_remove.hpp"
#include "fs_rename.hpp"
#include "fs_symlink.hpp"
#include "fs_unlink.hpp"
#include "fuse_symlink.hpp"
#include "ugid.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>


static
bool
_contains(const std::vector<Branch*> &haystack_,
          const char                 *needle_)
{
  for(auto &hay : haystack_)
    {
      if(hay->path == needle_)
        return true;
    }

  return false;
}

static
bool
_contains(const std::vector<Branch*> &haystack_,
          const std::string          &needle_)
{
  return ::_contains(haystack_,needle_.c_str());
}

static
void
_remove(const StrVec &toremove_)
{
  for(auto &path : toremove_)
    fs::remove(path);
}

static
int
_rename_create_path(const Policy::Search &searchPolicy_,
                    const Policy::Action &actionPolicy_,
                    const Branches::Ptr  &branches_,
                    const fs::path       &oldfusepath_,
                    const fs::path       &newfusepath_)
{
  int rv;
  Err err;
  StrVec toremove;
  std::vector<Branch*> newbranches;
  std::vector<Branch*> oldbranches;
  fs::path oldfullpath;
  fs::path newfullpath;

  rv = actionPolicy_(branches_,oldfusepath_,oldbranches);
  if(rv < 0)
    return rv;

  rv = searchPolicy_(branches_,newfusepath_.parent_path(),newbranches);
  if(rv < 0)
    return rv;

  for(auto &branch : *branches_)
    {
      newfullpath = branch.path / newfusepath_;

      if(!::_contains(oldbranches,branch.path))
        {
          toremove.push_back(newfullpath);
          continue;
        }

      oldfullpath = branch.path / oldfusepath_;

      rv = fs::rename(oldfullpath,newfullpath);
      if(rv < 0)
        {
          rv = fs::clonepath(newbranches[0]->path,
                             branch.path,
                             newfusepath_.parent_path());
          if(rv >= 0)
            rv = fs::rename(oldfullpath,newfullpath);
        }

      err = rv;
      if(rv < 0)
        toremove.push_back(oldfullpath);
    }

  if(err == 0)
    ::_remove(toremove);

  return err;
}

static
int
_rename_preserve_path(const Policy::Action &actionPolicy_,
                      const Branches::Ptr  &branches_,
                      const fs::path       &oldfusepath_,
                      const fs::path       &newfusepath_)
{
  int rv;
  bool success;
  StrVec toremove;
  std::vector<Branch*> oldbranches;
  fs::path oldfullpath;
  fs::path newfullpath;

  rv = actionPolicy_(branches_,oldfusepath_,oldbranches);
  if(rv < 0)
    return rv;

  success = false;
  for(auto &branch : *branches_)
    {
      newfullpath = branch.path / newfusepath_;

      if(!::_contains(oldbranches,branch.path))
        {
          toremove.push_back(newfullpath);
          continue;
        }

      oldfullpath = branch.path / oldfusepath_;

      rv = fs::rename(oldfullpath,newfullpath);
      if(rv < 0)
        {
          toremove.push_back(oldfullpath);
          continue;
        }

      success = true;
    }

  // TODO: probably should try to be nuanced here.
  if(success == false)
    return -EXDEV;

  ::_remove(toremove);

  return 0;
}

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
_rename_exdev_rename_target(const Policy::Action &actionPolicy_,
                            const Branches::Ptr  &ibranches_,
                            const fs::path       &oldfusepath_,
                            std::vector<Branch*> &obranches_)
{
  int rv;
  fs::path clonesrc;
  fs::path clonedst;

  rv = actionPolicy_(ibranches_,oldfusepath_,obranches_);
  if(rv < 0)
    return rv;

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
                          const Policy::Action &actionPolicy_,
                          const Branches::Ptr  &branches_,
                          const fs::path       &oldfusepath_,
                          const fs::path       &newfusepath_)
{
  int rv;
  fs::path target;
  fs::path linkpath;
  std::vector<Branch*> branches;

  rv = ::_rename_exdev_rename_target(actionPolicy_,branches_,oldfusepath_,branches);
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
                          const Policy::Action &actionPolicy_,
                          const Branches::Ptr  &branches_,
                          const fs::path       &mount_,
                          const fs::path       &oldfusepath_,
                          const fs::path       &newfusepath_)
{
  int rv;
  fs::path target;
  fs::path linkpath;
  std::vector<Branch*> branches;

  rv = ::_rename_exdev_rename_target(actionPolicy_,branches_,oldfusepath_,branches);
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
                                         cfg.func.rename.policy,
                                         cfg.branches,
                                         oldfusepath_,
                                         newfusepath_);
    case RenameEXDEV::ENUM::ABS_SYMLINK:
      return ::_rename_exdev_abs_symlink(ctx_,
                                         cfg.func.rename.policy,
                                         cfg.branches,
                                         cfg.mountpoint,
                                         oldfusepath_,
                                         newfusepath_);
    }

  return -EXDEV;
}

static
int
_rename(const fs::path &oldpath_,
        const fs::path &newpath_)
{
  if(cfg.func.create.policy.path_preserving() && !cfg.ignorepponrename)
    return ::_rename_preserve_path(cfg.func.rename.policy,
                                   cfg.branches,
                                   oldpath_,
                                   newpath_);

  return ::_rename_create_path(cfg.func.getattr.policy,
                               cfg.func.rename.policy,
                               cfg.branches,
                               oldpath_,
                               newpath_);
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
