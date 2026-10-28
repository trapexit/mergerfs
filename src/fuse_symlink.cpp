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

#include "fuse_symlink.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "error.hpp"
#include "fs_clonepath.hpp"
#include "fs_lchown.hpp"
#include "fs_lstat.hpp"
#include "fs_path.hpp"
#include "fs_inode.hpp"
#include "fs_symlink.hpp"
#include "fs_symlink_as.hpp"
#include "fs_unlink.hpp"
#include "fuse_getattr.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <sys/types.h>
#include <unistd.h>

enum class SymlinkMode
{
  CREATE_AS,
  PRESERVE_OWNER
};


static
int
_symlink_loop_core(const ugid_t      ugid_,
                   const SymlinkMode mode_,
                   const fs::path   &newbranch_,
                   const char       *target_,
                   const fs::path   &linkpath_,
                   struct stat      *st_)
{
  int rv;
  fs::path fullnewpath;

  fullnewpath = newbranch_ / linkpath_;

  if(mode_ == SymlinkMode::PRESERVE_OWNER)
    {
      struct stat owner{};

      owner.st_uid = ugid_.uid;
      owner.st_gid = ugid_.gid;

      rv = fs::symlink(target_,fullnewpath.native());
      if(rv < 0)
        return rv;

      rv = fs::lchown_check_on_error(fullnewpath.native(),owner);
      if(rv < 0)
        {
          fs::unlink(fullnewpath.native());
          return rv;
        }
    }
  else
    rv = fs::symlink_as(ugid_,target_,fullnewpath);

  if((rv >= 0) && (st_ != NULL) && (st_->st_ino == 0))
    {
      fs::lstat(fullnewpath,st_);
      if(st_->st_ino != 0)
        fs::inode::calc(newbranch_,
                        linkpath_,
                        st_);
    }

  return rv;
}

static
int
_symlink_loop(const ugid_t                ugid_,
              const SymlinkMode          mode_,
              const fs::path             &existingbranch_,
              const std::vector<Branch*> &newbranches_,
              const char                 *target_,
              const fs::path             &linkpath_,
              const fs::path             &newdirpath_,
              struct stat                *st_)
{
  int rv;
  Err err;

  for(auto &newbranch :newbranches_)
    {
      rv = fs::clonepath(existingbranch_,
                         newbranch->path,
                         newdirpath_);
      if(rv < 0)
        err = rv;
      else
        err = ::_symlink_loop_core(ugid_,
                                   mode_,
                                   newbranch->path,
                                   target_,
                                   linkpath_,
                                   st_);
    }

  return err;
}

static
int
_symlink(const ugid_t          ugid_,
         const SymlinkMode     mode_,
         const Policy::Search &searchFunc_,
         const Policy::Create &createFunc_,
         const Branches::Ptr   branches_,
         const char           *target_,
         const fs::path       &linkpath_,
         struct stat          *st_)
{
  int rv;
  fs::path newdirpath;
  std::vector<Branch*> newbranches;
  std::vector<Branch*> existingbranches;

  newdirpath = linkpath_.parent_path();

  rv = searchFunc_(branches_,newdirpath,existingbranches);
  if(rv < 0)
    return rv;
  if(existingbranches.empty())
    return -ENOENT;

  rv = createFunc_(branches_,newdirpath,newbranches);
  if(rv < 0)
    return rv;
  if(newbranches.empty())
    return -ENOENT;

  return ::_symlink_loop(ugid_,
                         mode_,
                         existingbranches[0]->path,
                         newbranches,
                         target_,
                         linkpath_,
                         newdirpath,
                         st_);
}

static
int
_symlink_with_config(const ugid_t      ugid_,
                     const SymlinkMode mode_,
                     const char       *target_,
                     const fs::path   &linkpath_,
                     struct stat      *st_,
                     fuse_timeouts_t  *timeouts_)
{
  int rv;

  rv = ::_symlink(ugid_,
                  mode_,
                  cfg.func.getattr.policy,
                  cfg.func.symlink.policy,
                  cfg.branches,
                  target_,
                  linkpath_,
                  st_);
  if(rv == -EROFS)
    {
      cfg.branches.find_and_set_mode_ro();
      rv = ::_symlink(ugid_,
                      mode_,
                      cfg.func.getattr.policy,
                      cfg.func.symlink.policy,
                      cfg.branches,
                      target_,
                      linkpath_,
                      st_);
    }

  if(timeouts_ != NULL)
    {
      switch(cfg.follow_symlinks)
        {
        case FollowSymlinks::ENUM::NEVER:
          timeouts_->entry = ((rv >= 0) ?
                              cfg.cache_entry :
                              cfg.cache_negative_entry);
          timeouts_->attr  = cfg.cache_attr;
          break;
        default:
          timeouts_->entry = 0;
          timeouts_->attr  = 0;
          break;
        }
    }

  return rv;
}

int
FUSE::symlink_owned(const ugid_t      owner_,
                    const char       *target_,
                    const fs::path   &linkpath_,
                    struct stat      *st_,
                    fuse_timeouts_t  *timeouts_)
{
  return ::_symlink_with_config(owner_,
                                SymlinkMode::PRESERVE_OWNER,
                                target_,
                                linkpath_,
                                st_,
                                timeouts_);
}

int
FUSE::symlink(const fuse_req_ctx_t *ctx_,
              const char           *target_,
              const char           *linkpath_,
              struct stat          *st_,
              fuse_timeouts_t      *timeouts_)
{
  const fs::path linkpath{linkpath_};

  return ::_symlink_with_config(ctx_,
                                SymlinkMode::CREATE_AS,
                                target_,
                                linkpath,
                                st_,
                                timeouts_);
}
