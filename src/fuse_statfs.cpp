/*
  Copyright (c) 2018, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_statfs.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fs_lstat.hpp"
#include "fs_path.hpp"
#include "fs_lstatvfs.hpp"
#include "statvfs_util.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <filesystem>
#include <algorithm>
#include <limits>
#include <map>
#include <vector>


static
void
_normalize_statvfs(struct statvfs      *fsstat_,
                   const unsigned long  min_bsize_,
                   const unsigned long  min_frsize_,
                   const unsigned long  min_namemax_)
{
  fsstat_->f_blocks  = (fsblkcnt_t)((fsstat_->f_blocks * fsstat_->f_frsize) / min_frsize_);
  fsstat_->f_bfree   = (fsblkcnt_t)((fsstat_->f_bfree  * fsstat_->f_frsize) / min_frsize_);
  fsstat_->f_bavail  = (fsblkcnt_t)((fsstat_->f_bavail * fsstat_->f_frsize) / min_frsize_);
  fsstat_->f_bsize   = min_bsize_;
  fsstat_->f_frsize  = min_frsize_;
  fsstat_->f_namemax = min_namemax_;
}

static
void
_merge_statvfs(struct statvfs       * const out_,
               const struct statvfs * const in_)
{
  out_->f_blocks += in_->f_blocks;
  out_->f_bfree  += in_->f_bfree;
  out_->f_bavail += in_->f_bavail;

  out_->f_files  += in_->f_files;
  out_->f_ffree  += in_->f_ffree;
  out_->f_favail += in_->f_favail;
}

static
bool
_should_ignore(const StatFSIgnore  ignore_,
               const Branch       &branch_,
               const bool          readonly_)
{
  return ((((ignore_ == StatFSIgnore::ENUM::RO) || readonly_) &&
           (branch_.ro_or_nc())) ||
          ((ignore_ == StatFSIgnore::ENUM::NC) && (branch_.nc())));
}

static
int
_statfs(const Branches::Ptr &branches_,
        const fs::path      &fusepath_,
        const StatFS         mode_,
        const StatFSIgnore   ignore_,
        struct statvfs      *fsstat_)
{
  int rv;
  struct stat st;
  struct statvfs stvfs;
  unsigned long min_bsize;
  unsigned long min_frsize;
  unsigned long min_namemax;
  std::map<dev_t,struct statvfs> fsstats;
  fs::path fullpath;

  min_bsize   = std::numeric_limits<unsigned long>::max();
  min_frsize  = std::numeric_limits<unsigned long>::max();
  min_namemax = std::numeric_limits<unsigned long>::max();
  for(const auto &branch : *branches_)
    {
      if(mode_ == StatFS::ENUM::FULL)
        fullpath = branch.path / fusepath_;
      else
        fullpath = branch.path;

      rv = fs::lstat(fullpath,&st);
      if(rv < 0)
        continue;

      rv = fs::lstatvfs(fullpath,&stvfs);
      if(rv < 0)
        continue;

      if(stvfs.f_bsize   && (min_bsize   > stvfs.f_bsize))
        min_bsize = stvfs.f_bsize;
      if(stvfs.f_frsize  && (min_frsize  > stvfs.f_frsize))
        min_frsize = stvfs.f_frsize;
      if(stvfs.f_namemax && (min_namemax > stvfs.f_namemax))
        min_namemax = stvfs.f_namemax;

      if(::_should_ignore(ignore_,branch,StatVFS::readonly(stvfs)))
        {
          stvfs.f_bavail = 0;
          stvfs.f_favail = 0;
        }

      fsstats.insert(std::make_pair(st.st_dev,stvfs));
    }

  std::map<dev_t,struct statvfs>::iterator iter    = fsstats.begin();
  std::map<dev_t,struct statvfs>::iterator enditer = fsstats.end();
  if(iter != enditer)
    {
      *fsstat_ = iter->second;
      ::_normalize_statvfs(fsstat_,min_bsize,min_frsize,min_namemax);

      for(++iter; iter != enditer; ++iter)
        {
          ::_normalize_statvfs(&iter->second,min_bsize,min_frsize,min_namemax);
          ::_merge_statvfs(fsstat_,&iter->second);
        }
    }

  return 0;
}

int
FUSE::statfs(const char     *fusepath_,
             struct statvfs *st_)
{
  const fs::path      fusepath{fusepath_};
  const fuse_context *fc = fuse_get_context();
  const ugid::Set     ugid(fc->uid,fc->gid);

  return ::_statfs(cfg.branches,
                   fusepath,
                   cfg.statfs,
                   cfg.statfs_ignore,
                   st_);
}
