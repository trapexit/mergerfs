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

#include "fuse_symlink.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "error.hpp"
#include "fs_clonepath.hpp"
#include "fs_lstat.hpp"
#include "fs_path.hpp"
#include "fs_inode.hpp"
#include "fs_symlink.hpp"
#include "fuse_getattr.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>

#include <sys/types.h>
#include <unistd.h>

using std::string;


static
int
_symlink_loop_core(const std::string &newbranch_,
                   const char        *target_,
                   const char        *linkpath_,
                   struct stat       *st_)
{
  int rv;
  string fullnewpath;

  fullnewpath = fs::path::make(newbranch_,linkpath_);

  rv = fs::symlink(target_,fullnewpath);
  if((rv >= 0) && (st_ != NULL) && (st_->st_ino == 0))
    {
      fs::lstat(fullnewpath,st_);
      if(st_->st_ino != 0)
        fs::inode::calc(newbranch_,linkpath_,st_);
    }

  return rv;
}

static
int
_symlink_loop(const std::string          &existingbranch_,
              const std::vector<Branch*> &newbranches_,
              const char                 *target_,
              const char                 *linkpath_,
              const std::string          &newdirpath_,
              struct stat                *st_)
{
  int rv;
  Err err;

  for(auto &newbranch :newbranches_)
    {
      rv = fs::clonepath_as_root(existingbranch_,
                                 newbranch->path,
                                 newdirpath_);
      if(rv < 0)
        err = rv;
      else
        err = ::_symlink_loop_core(newbranch->path,
                                   target_,
                                   linkpath_,
                                   st_);
    }

  return err;
}

static
int
_symlink(const Policy::Search &searchFunc_,
         const Policy::Create &createFunc_,
         const Branches       &branches_,
         const char           *target_,
         const char           *linkpath_,
         struct stat          *st_)
{
  int rv;
  string newdirpath;
  std::vector<Branch*> newbranches;
  std::vector<Branch*> existingbranches;

  newdirpath = fs::path::dirname(linkpath_);

  rv = searchFunc_(branches_,newdirpath,existingbranches);
  if(rv < 0)
    return rv;

  rv = createFunc_(branches_,newdirpath,newbranches);
  if(rv < 0)
    return rv;

  return ::_symlink_loop(existingbranches[0]->path,
                         newbranches,
                         target_,
                         linkpath_,
                         newdirpath,
                         st_);
}

int
FUSE::symlink(const char      *target_,
              const char      *linkpath_,
              struct stat     *st_,
              fuse_timeouts_t *timeouts_)
{
  int rv;
  Config::Read cfg;
  const fuse_context *fc  = fuse_get_context();
  const ugid::Set     ugid(fc->uid,fc->gid);

  rv = ::_symlink(cfg->func.getattr.policy,
                  cfg->func.symlink.policy,
                  cfg->branches,
                  target_,
                  linkpath_,
                  st_);
  if(rv == -EROFS)
    {
      Config::Write()->branches.find_and_set_mode_ro();
      rv = ::_symlink(cfg->func.getattr.policy,
                      cfg->func.symlink.policy,
                      cfg->branches,
                      target_,
                      linkpath_,
                      st_);
    }

  if(timeouts_ != NULL)
    {
      switch(cfg->follow_symlinks)
        {
        case FollowSymlinks::ENUM::NEVER:
          timeouts_->entry = ((rv >= 0) ?
                              cfg->cache_entry :
                              cfg->cache_negative_entry);
          timeouts_->attr  = cfg->cache_attr;
          break;
        default:
          timeouts_->entry = 0;
          timeouts_->attr  = 0;
          break;
        }
    }

  return rv;
}
