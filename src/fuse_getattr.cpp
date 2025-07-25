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

#include "fuse_getattr.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fs_fstat.hpp"
#include "fs_inode.hpp"
#include "fs_lstat.hpp"
#include "fs_path.hpp"
#include "fs_stat.hpp"
#include "fuse_fgetattr.hpp"
#include "state.hpp"
#include "str.hpp"
#include "symlinkify.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>

using std::string;


static
void
_set_stat_if_leads_to_dir(const std::string &path_,
                          struct stat       *st_)
{
  int rv;
  struct stat st;

  rv = fs::stat(path_,&st);
  if(rv < 0)
    return;

  if(S_ISDIR(st.st_mode))
    *st_ = st;

  return;
}

static
void
_set_stat_if_leads_to_reg(const std::string &path_,
                          struct stat       *st_)
{
  int rv;
  struct stat st;

  rv = fs::stat(path_,&st);
  if(rv < 0)
    return;

  if(S_ISREG(st.st_mode))
    *st_ = st;

  return;
}

static
int
_getattr_fake_root(struct stat *st_)
{
  st_->st_dev     = 0;
  st_->st_ino     = 0;
  st_->st_mode    = (S_IFDIR|S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
  st_->st_nlink   = 2;
  st_->st_uid     = 0;
  st_->st_gid     = 0;
  st_->st_rdev    = 0;
  st_->st_size    = 0;
  st_->st_blksize = 512;
  st_->st_blocks  = 0;
  st_->st_atime   = 0;
  st_->st_mtime   = 0;
  st_->st_ctime   = 0;

  return 0;
}

static
int
_getattr_controlfile(struct stat *st_)
{
  static const uid_t  uid = ::getuid();
  static const gid_t  gid = ::getgid();
  static const time_t now = ::time(NULL);

  st_->st_dev     = 0;
  st_->st_ino     = 0;
  st_->st_mode    = (S_IFREG|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
  st_->st_nlink   = 1;
  st_->st_uid     = uid;
  st_->st_gid     = gid;
  st_->st_rdev    = 0;
  st_->st_size    = 0;
  st_->st_blksize = 512;
  st_->st_blocks  = 0;
  st_->st_atime   = now;
  st_->st_mtime   = now;
  st_->st_ctime   = now;

  return 0;
}

static
int
_getattr(const Policy::Search &searchFunc_,
         const Branches       &branches_,
         const char           *fusepath_,
         struct stat          *st_,
         const bool            symlinkify_,
         const time_t          symlinkify_timeout_,
         FollowSymlinks        followsymlinks_)
{
  int rv;
  string fullpath;
  std::vector<Branch*> branches;

  rv = searchFunc_(branches_,fusepath_,branches);
  if(rv < 0)
    return rv;

  fullpath = fs::path::make(branches[0]->path,fusepath_);

  switch(followsymlinks_)
    {
    case FollowSymlinks::ENUM::NEVER:
      rv = fs::lstat(fullpath,st_);
      break;
    case FollowSymlinks::ENUM::DIRECTORY:
      rv = fs::lstat(fullpath,st_);
      if(S_ISLNK(st_->st_mode))
        ::_set_stat_if_leads_to_dir(fullpath,st_);
      break;
    case FollowSymlinks::ENUM::REGULAR:
      rv = fs::lstat(fullpath,st_);
      if(S_ISLNK(st_->st_mode))
        ::_set_stat_if_leads_to_reg(fullpath,st_);
      break;
    case FollowSymlinks::ENUM::ALL:
      rv = fs::stat(fullpath,st_);
      if(rv < 0)
        rv = fs::lstat(fullpath,st_);
      break;
    }

  if(rv < 0)
    return rv;

  if(symlinkify_ && symlinkify::can_be_symlink(*st_,symlinkify_timeout_))
    symlinkify::convert(fullpath,st_);

  fs::inode::calc(branches[0]->path,fusepath_,st_);

  return 0;
}

int
_getattr(const char      *fusepath_,
         struct stat     *st_,
         fuse_timeouts_t *timeout_)
{
  int rv;
  Config::Read cfg;
  const fuse_context *fc = fuse_get_context();
  const ugid::Set     ugid(fc->uid,fc->gid);

  rv = ::_getattr(cfg->func.getattr.policy,
                  cfg->branches,
                  fusepath_,
                  st_,
                  cfg->symlinkify,
                  cfg->symlinkify_timeout,
                  cfg->follow_symlinks);
  if((rv < 0) && Config::is_rootdir(fusepath_))
    return ::_getattr_fake_root(st_);

  timeout_->entry = ((rv >= 0) ?
                     cfg->cache_entry :
                     cfg->cache_negative_entry);
  timeout_->attr  = cfg->cache_attr;

  return rv;
}

int
FUSE::getattr(const char      *fusepath_,
              struct stat     *st_,
              fuse_timeouts_t *timeout_)
{
  if(Config::is_ctrl_file(fusepath_))
    return ::_getattr_controlfile(st_);

  return ::_getattr(fusepath_,st_,timeout_);
}
