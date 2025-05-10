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
#include "fs_lstat.hpp"
#include "fs_path.hpp"
#include "fuse_getattr.hpp"
#include "fuse_symlink.hpp"
#include "ghc/filesystem.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>
#include <vector>


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
  int
  link_create_path_loop(const std::vector<Branch*> &oldbranches_,
                        const Branch               *newbranch_,
                        const char                 *oldfusepath_,
                        const char                 *newfusepath_,
                        const std::string          &newfusedirpath_)
  {
    int rv;
    int error;
    std::string oldfullpath;
    std::string newfullpath;

    error = -1;
    for(auto &oldbranch : oldbranches_)
      {
        oldfullpath = fs::path::make(oldbranch->path,oldfusepath_);
        newfullpath = fs::path::make(oldbranch->path,newfusepath_);

        rv = fs::link(oldfullpath,newfullpath);
        if((rv == -1) && (errno == ENOENT))
          {
            rv = fs::clonepath_as_root(newbranch_->path,oldbranch->path,newfusedirpath_);
            if(rv == 0)
              rv = fs::link(oldfullpath,newfullpath);
          }

        error = error::calc(rv,error,errno);
      }

    return -error;
  }

  static
  int
  link_create_path(const Policy::Search &searchFunc_,
                   const Policy::Action &actionFunc_,
                   const Branches       &ibranches_,
                   const char           *oldfusepath_,
                   const char           *newfusepath_)
  {
    int rv;
    std::string newfusedirpath;
    std::vector<Branch*> oldbranches;
    std::vector<Branch*> newbranches;

    rv = actionFunc_(ibranches_,oldfusepath_,oldbranches);
    if(rv == -1)
      return -errno;

    newfusedirpath = fs::path::dirname(newfusepath_);

    rv = searchFunc_(ibranches_,newfusedirpath,newbranches);
    if(rv == -1)
      return -errno;

    return l::link_create_path_loop(oldbranches,newbranches[0],
                                    oldfusepath_,newfusepath_,
                                    newfusedirpath);
  }

  static
  int
  link_preserve_path_core(const std::string &oldbasepath_,
                          const char        *oldfusepath_,
                          const char        *newfusepath_,
                          struct stat       *st_,
                          const int          error_)
  {
    int rv;
    std::string oldfullpath;
    std::string newfullpath;

    oldfullpath = fs::path::make(oldbasepath_,oldfusepath_);
    newfullpath = fs::path::make(oldbasepath_,newfusepath_);

    rv = fs::link(oldfullpath,newfullpath);
    if((rv == -1) && (errno == ENOENT))
      errno = EXDEV;
    if((rv == 0) && (st_->st_ino == 0))
      rv = fs::lstat(oldfullpath,st_);

    return error::calc(rv,error_,errno);
  }

  static
  int
  link_preserve_path_loop(const std::vector<Branch*> &oldbranches_,
                          const char            *oldfusepath_,
                          const char            *newfusepath_,
                          struct stat           *st_)
  {
    int error;

    error = -1;
    for(auto &oldbranch : oldbranches_)
      {
        error = l::link_preserve_path_core(oldbranch->path,
                                           oldfusepath_,
                                           newfusepath_,
                                           st_,
                                           error);
      }

    return -error;
  }

  static
  int
  link_preserve_path(const Policy::Action &actionFunc_,
                     const Branches       &branches_,
                     const char           *oldfusepath_,
                     const char           *newfusepath_,
                     struct stat          *st_)
  {
    int rv;
    std::vector<Branch*> oldbranches;

    rv = actionFunc_(branches_,oldfusepath_,oldbranches);
    if(rv == -1)
      return -errno;

    return l::link_preserve_path_loop(oldbranches,
                                      oldfusepath_,
                                      newfusepath_,
                                      st_);
  }

  static
  int
  link(Config::Read    &cfg_,
       const char      *oldpath_,
       const char      *newpath_,
       struct stat     *st_)
  {
    if(cfg_->func.create.policy.path_preserving() && !cfg_->ignorepponrename)
      return l::link_preserve_path(cfg_->func.link.policy,
                                   cfg_->branches,
                                   oldpath_,
                                   newpath_,
                                   st_);

    return l::link_create_path(cfg_->func.getattr.policy,
                               cfg_->func.link.policy,
                               cfg_->branches,
                               oldpath_,
                               newpath_);
  }

  static
  int
  link(Config::Read    &cfg_,
       const char      *oldpath_,
       const char      *newpath_,
       struct stat     *st_,
       fuse_timeouts_t *timeouts_)
  {
    int rv;

    rv = l::link(cfg_,oldpath_,newpath_,st_);
    if(rv < 0)
      return rv;

    return FUSE::getattr(newpath_,st_,timeouts_);
  }

  static
  int
  link_exdev_rel_symlink(const char      *oldpath_,
                         const char      *newpath_,
                         struct stat     *st_,
                         fuse_timeouts_t *timeouts_)
  {
    int rv;
    fs::Path target(oldpath_);
    fs::Path linkpath(newpath_);

    target = target.lexically_relative(linkpath.parent_path());

    rv = FUSE::symlink(target.c_str(),linkpath.c_str());
    if(rv == 0)
      rv = FUSE::getattr(oldpath_,st_,timeouts_);

    // Disable caching since we created a symlink but should be a regular.
    timeouts_->attr  = 0;
    timeouts_->entry = 0;

    return rv;
  }

  static
  int
  link_exdev_abs_base_symlink(const Policy::Search &openPolicy_,
                              const Branches::Ptr  &ibranches_,
                              const char           *oldpath_,
                              const char           *newpath_,
                              struct stat          *st_,
                              fuse_timeouts_t      *timeouts_)
  {
    int rv;
    std::string target;
    std::vector<Branch*> obranches;

    rv = openPolicy_(ibranches_,oldpath_,obranches);
    if(rv == -1)
      return -errno;

    target = fs::path::make(obranches[0]->path,oldpath_);

    rv = FUSE::symlink(target.c_str(),newpath_);
    if(rv == 0)
      rv = FUSE::getattr(oldpath_,st_,timeouts_);

    // Disable caching since we created a symlink but should be a regular.
    timeouts_->attr  = 0;
    timeouts_->entry = 0;

    return rv;
  }

  static
  int
  link_exdev_abs_pool_symlink(const fs::Path   mount_,
                              const char      *oldpath_,
                              const char      *newpath_,
                              struct stat     *st_,
                              fuse_timeouts_t *timeouts_)
  {
    int rv;
    StrVec basepaths;
    std::string target;

    target = fs::path::make(mount_,oldpath_);

    rv = FUSE::symlink(target.c_str(),newpath_);
    if(rv == 0)
      rv = FUSE::getattr(oldpath_,st_,timeouts_);

    // Disable caching since we created a symlink but should be a regular.
    timeouts_->attr  = 0;
    timeouts_->entry = 0;

    return rv;
  }

  static
  int
  link_exdev(Config::Read    &cfg_,
             const char      *oldpath_,
             const char      *newpath_,
             struct stat     *st_,
             fuse_timeouts_t *timeouts_)
  {
    switch(cfg_->link_exdev)
      {
      case LinkEXDEV::ENUM::PASSTHROUGH:
        return -EXDEV;
      case LinkEXDEV::ENUM::REL_SYMLINK:
        return l::link_exdev_rel_symlink(oldpath_,
                                         newpath_,
                                         st_,
                                         timeouts_);
      case LinkEXDEV::ENUM::ABS_BASE_SYMLINK:
        return l::link_exdev_abs_base_symlink(cfg_->func.open.policy,
                                              cfg_->branches,
                                              oldpath_,
                                              newpath_,
                                              st_,
                                              timeouts_);
      case LinkEXDEV::ENUM::ABS_POOL_SYMLINK:
        return l::link_exdev_abs_pool_symlink(cfg_->mountpoint,
                                              oldpath_,
                                              newpath_,
                                              st_,
                                              timeouts_);
      }

    return -EXDEV;
  }
}

namespace FUSE
{
  int
  link(const char      *oldpath_,
       const char      *newpath_,
       struct stat     *st_,
       fuse_timeouts_t *timeouts_)
  {
    int rv;
    Config::Read cfg;
    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    rv = l::link(cfg,oldpath_,newpath_,st_,timeouts_);
    if(rv == -EXDEV)
      rv = l::link_exdev(cfg,oldpath_,newpath_,st_,timeouts_);

    return rv;
  }
}
