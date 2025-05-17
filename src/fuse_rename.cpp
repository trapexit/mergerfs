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
#include "fs_clonepath.hpp"
#include "fs_link.hpp"
#include "fs_mkdir_as_root.hpp"
#include "fs_path.hpp"
#include "fs_remove.hpp"
#include "fs_rename.hpp"
#include "fs_symlink.hpp"
#include "fs_unlink.hpp"
#include "fuse_symlink.hpp"
#include "ugid.hpp"

#include "ghc/filesystem.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <iostream>

namespace gfs = ghc::filesystem;

namespace error
{
  static
  inline
  int
  calc(const int rv_,
       const int prev_,
       const int cur_)
  {
    if(rv_ == -1)
      {
        if(prev_ == 0)
          return 0;
        return cur_;
      }

    return 0;
  }
}

namespace l
{
  static
  bool
  contains(const std::vector<Branch*> &haystack_,
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
  contains(const std::vector<Branch*> &haystack_,
           const std::string          &needle_)
  {
    return l::contains(haystack_,needle_.c_str());
  }

  static
  void
  remove(const StrVec &toremove_)
  {
    for(auto &path : toremove_)
      fs::remove(path);
  }

  static
  int
  rename_create_path(const Policy::Search &searchPolicy_,
                     const Policy::Action &actionPolicy_,
                     const Branches::Ptr  &branches_,
                     const gfs::path      &oldfusepath_,
                     const gfs::path      &newfusepath_)
  {
    int rv;
    int error;
    StrVec toremove;
    std::vector<Branch*> newbranches;
    std::vector<Branch*> oldbranches;
    gfs::path oldfullpath;
    gfs::path newfullpath;

    rv = actionPolicy_(branches_,oldfusepath_,oldbranches);
    if(rv == -1)
      return -errno;

    rv = searchPolicy_(branches_,newfusepath_.parent_path(),newbranches);
    if(rv == -1)
      return -errno;

    error = -1;
    for(auto &branch : *branches_)
      {
        newfullpath  = branch.path;
        newfullpath += newfusepath_;

        if(!l::contains(oldbranches,branch.path))
          {
            toremove.push_back(newfullpath);
            continue;
          }

        oldfullpath  = branch.path;
        oldfullpath += oldfusepath_;

        rv = fs::rename(oldfullpath,newfullpath);
        if(rv == -1)
          {
            rv = fs::clonepath_as_root(newbranches[0]->path,
                                       branch.path,
                                       newfusepath_.parent_path());
            if(rv == 0)
              rv = fs::rename(oldfullpath,newfullpath);
          }

        error = error::calc(rv,error,errno);
        if(rv == -1)
          toremove.push_back(oldfullpath);
      }

    if(error == 0)
      l::remove(toremove);

    return -error;
  }

  static
  int
  rename_preserve_path(const Policy::Action &actionPolicy_,
                       const Branches::Ptr  &branches_,
                       const gfs::path      &oldfusepath_,
                       const gfs::path      &newfusepath_)
  {
    int rv;
    bool success;
    StrVec toremove;
    std::vector<Branch*> oldbranches;
    gfs::path oldfullpath;
    gfs::path newfullpath;

    rv = actionPolicy_(branches_,oldfusepath_,oldbranches);
    if(rv == -1)
      return -errno;

    success = false;
    for(auto &branch : *branches_)
      {
        newfullpath  = branch.path;
        newfullpath += newfusepath_;

        if(!l::contains(oldbranches,branch.path))
          {
            toremove.push_back(newfullpath);
            continue;
          }

        oldfullpath  = branch.path;
        oldfullpath += oldfusepath_;

        rv = fs::rename(oldfullpath,newfullpath);
        if(rv == -1)
          {
            toremove.push_back(oldfullpath);
            continue;
          }

        success = true;
      }

    // TODO: probably should try to be nuanced here.
    if(success == false)
      return -EXDEV;

    l::remove(toremove);

    return 0;
  }

  static
  void
  rename_exdev_rename_back(const std::vector<Branch*> &branches_,
                           const gfs::path       &oldfusepath_)
  {
    gfs::path oldpath;
    gfs::path newpath;

    for(auto &branch : branches_)
      {
        oldpath  = branch->path;
        oldpath /= ".mergerfs_rename_exdev";
        oldpath += oldfusepath_;

        newpath  = branch->path;
        newpath += oldfusepath_;

        fs::rename(oldpath,newpath);
      }
  }

  static
  int
  rename_exdev_rename_target(const Policy::Action &actionPolicy_,
                             const Branches::Ptr  &ibranches_,
                             const gfs::path      &oldfusepath_,
                             std::vector<Branch*> &obranches_)
  {
    int rv;
    gfs::path clonesrc;
    gfs::path clonetgt;

    rv = actionPolicy_(ibranches_,oldfusepath_,obranches_);
    if(rv == -1)
      return -errno;

    ugid::SetRootGuard ugidGuard;
    for(auto &branch : obranches_)
      {
        clonesrc  = branch->path;
        clonetgt  = branch->path;
        clonetgt /= ".mergerfs_rename_exdev";

        rv = fs::clonepath(clonesrc,clonetgt,oldfusepath_.parent_path());
        if((rv == -1) && (errno == ENOENT))
          {
            fs::mkdir(clonetgt,01777);
            rv = fs::clonepath(clonesrc,clonetgt,oldfusepath_.parent_path());
          }

        if(rv == -1)
          goto error;

        clonesrc += oldfusepath_;
        clonetgt += oldfusepath_;

        rv = fs::rename(clonesrc,clonetgt);
        if(rv == -1)
          goto error;
      }

    return 0;

  error:
    l::rename_exdev_rename_back(obranches_,oldfusepath_);

    return -EXDEV;
  }

  static
  int
  rename_exdev_rel_symlink(const Policy::Action &actionPolicy_,
                           const Branches::Ptr  &branches_,
                           const gfs::path      &oldfusepath_,
                           const gfs::path      &newfusepath_)
  {
    int rv;
    gfs::path target;
    gfs::path linkpath;
    std::vector<Branch*> branches;

    rv = l::rename_exdev_rename_target(actionPolicy_,branches_,oldfusepath_,branches);
    if(rv < 0)
      return rv;

    linkpath  = newfusepath_;
    target    = "/.mergerfs_rename_exdev";
    target   += oldfusepath_;
    target    = target.lexically_relative(linkpath.parent_path());

    rv = FUSE::symlink(target.c_str(),linkpath.c_str());
    if(rv < 0)
      l::rename_exdev_rename_back(branches,oldfusepath_);

    return rv;
  }

  static
  int
  rename_exdev_abs_symlink(const Policy::Action &actionPolicy_,
                           const Branches::Ptr  &branches_,
                           const gfs::path      &mount_,
                           const gfs::path      &oldfusepath_,
                           const gfs::path      &newfusepath_)
  {
    int rv;
    gfs::path target;
    gfs::path linkpath;
    std::vector<Branch*> branches;

    rv = l::rename_exdev_rename_target(actionPolicy_,branches_,oldfusepath_,branches);
    if(rv < 0)
      return rv;

    linkpath  = newfusepath_;
    target    = mount_;
    target   /= ".mergerfs_rename_exdev";
    target   += oldfusepath_;

    rv = FUSE::symlink(target.c_str(),linkpath.c_str());
    if(rv < 0)
      l::rename_exdev_rename_back(branches,oldfusepath_);

    return rv;
  }

  static
  int
  rename_exdev(Config::Read    &cfg_,
               const gfs::path &oldfusepath_,
               const gfs::path &newfusepath_)
  {
    switch(cfg_->rename_exdev)
      {
      case RenameEXDEV::ENUM::PASSTHROUGH:
        return -EXDEV;
      case RenameEXDEV::ENUM::REL_SYMLINK:
        return l::rename_exdev_rel_symlink(cfg_->func.rename.policy,
                                           cfg_->branches,
                                           oldfusepath_,
                                           newfusepath_);
      case RenameEXDEV::ENUM::ABS_SYMLINK:
        return l::rename_exdev_abs_symlink(cfg_->func.rename.policy,
                                           cfg_->branches,
                                           cfg_->mountpoint,
                                           oldfusepath_,
                                           newfusepath_);
      }

    return -EXDEV;
  }

  static
  int
  rename(Config::Read    &cfg_,
         const gfs::path &oldpath_,
         const gfs::path &newpath_)
  {
    if(cfg_->func.create.policy.path_preserving() && !cfg_->ignorepponrename)
      return l::rename_preserve_path(cfg_->func.rename.policy,
                                     cfg_->branches,
                                     oldpath_,
                                     newpath_);

    return l::rename_create_path(cfg_->func.getattr.policy,
                                 cfg_->func.rename.policy,
                                 cfg_->branches,
                                 oldpath_,
                                 newpath_);
  }
}

namespace FUSE
{
  int
  rename(const char *oldfusepath_,
         const char *newfusepath_)
  {
    int rv;
    Config::Read cfg;
    gfs::path oldfusepath(oldfusepath_);
    gfs::path newfusepath(newfusepath_);
    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    rv = l::rename(cfg,oldfusepath,newfusepath);
    if(rv == -EXDEV)
      return l::rename_exdev(cfg,oldfusepath,newfusepath);

    return rv;
  }
}
