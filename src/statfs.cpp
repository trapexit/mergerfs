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

#include "config.hpp"
#include "errno.hpp"
#include "fs_base_stat.hpp"
#include "fs_base_statvfs.hpp"
#include "fs_path.hpp"
#include "rwlock.hpp"
#include "statvfs_util.hpp"
#include "ugid.hpp"

#include <fuse.h>

#include <algorithm>
#include <limits>
#include <map>
#include <string>
#include <vector>

using std::string;
using std::map;
using std::vector;
using mergerfs::Config;
typedef Config::StatFS StatFS;
typedef Config::StatFSIgnore StatFSIgnore;

static
void
_normalize_statvfs(struct statvfs      *fsstat,
                   const unsigned long  min_bsize,
                   const unsigned long  min_frsize,
                   const unsigned long  min_namemax)
{
  fsstat->f_blocks  = (fsblkcnt_t)((fsstat->f_blocks * fsstat->f_frsize) / min_frsize);
  fsstat->f_bfree   = (fsblkcnt_t)((fsstat->f_bfree  * fsstat->f_frsize) / min_frsize);
  fsstat->f_bavail  = (fsblkcnt_t)((fsstat->f_bavail * fsstat->f_frsize) / min_frsize);
  fsstat->f_bsize   = min_bsize;
  fsstat->f_frsize  = min_frsize;
  fsstat->f_namemax = min_namemax;
}

static
void
_merge_statvfs(struct statvfs       * const out,
               const struct statvfs * const in)
{
  out->f_blocks += in->f_blocks;
  out->f_bfree  += in->f_bfree;
  out->f_bavail += in->f_bavail;

  out->f_files  += in->f_files;
  out->f_ffree  += in->f_ffree;
  out->f_favail += in->f_favail;
}

static
bool
_should_ignore(const StatFSIgnore::Enum  ignore_,
               const Branch             *branch_,
               const bool                readonly_)
{
  return ((((ignore_ == StatFSIgnore::RO) || readonly_) &&
           (branch_->ro_or_nc())) ||
          ((ignore_ == StatFSIgnore::NC) && (branch_->nc())));
}

static
int
_statfs(const Branches           &branches_,
        const char               *fusepath,
        const StatFS::Enum        mode,
        const StatFSIgnore::Enum  ignore,
        struct statvfs           &fsstat)
{
  int rv;
  string fullpath;
  struct stat st;
  struct statvfs stvfs;
  unsigned long min_bsize;
  unsigned long min_frsize;
  unsigned long min_namemax;
  map<dev_t,struct statvfs> fsstats;

  min_bsize   = std::numeric_limits<unsigned long>::max();
  min_frsize  = std::numeric_limits<unsigned long>::max();
  min_namemax = std::numeric_limits<unsigned long>::max();
  for(size_t i = 0, ei = branches_.size(); i < ei; i++)
    {
      fullpath = ((mode == StatFS::FULL) ?
                  fs::path::make(&branches_[i].path,fusepath) :
                  branches_[i].path);

      rv = fs::lstat(fullpath,st);
      if(rv == -1)
        continue;

      rv = fs::lstatvfs(fullpath,&stvfs);
      if(rv == -1)
        continue;

      if(stvfs.f_bsize   && (min_bsize   > stvfs.f_bsize))
        min_bsize = stvfs.f_bsize;
      if(stvfs.f_frsize  && (min_frsize  > stvfs.f_frsize))
        min_frsize = stvfs.f_frsize;
      if(stvfs.f_namemax && (min_namemax > stvfs.f_namemax))
        min_namemax = stvfs.f_namemax;

      if(_should_ignore(ignore,&branches_[i],StatVFS::readonly(stvfs)))
        {
          stvfs.f_bavail = 0;
          stvfs.f_favail = 0;
        }

      fsstats.insert(std::make_pair(st.st_dev,stvfs));
    }

  map<dev_t,struct statvfs>::iterator iter    = fsstats.begin();
  map<dev_t,struct statvfs>::iterator enditer = fsstats.end();
  if(iter != enditer)
    {
      fsstat = iter->second;
      _normalize_statvfs(&fsstat,min_bsize,min_frsize,min_namemax);

      for(++iter; iter != enditer; ++iter)
        {
          _normalize_statvfs(&iter->second,min_bsize,min_frsize,min_namemax);
          _merge_statvfs(&fsstat,&iter->second);
        }
    }

  return 0;
}

namespace mergerfs
{
  namespace fuse
  {
    int
    statfs(const char     *fusepath,
           struct statvfs *stat)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.branches_lock);

      return _statfs(config.branches,
                     fusepath,
                     config.statfs,
                     config.statfs_ignore,
                     *stat);
    }
  }
}
