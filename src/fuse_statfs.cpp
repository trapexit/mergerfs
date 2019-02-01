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
typedef Config::StatFS StatFS;
typedef Config::StatFSIgnore StatFSIgnore;

namespace l
{
  static
  void
  normalize_statvfs(struct statvfs      *fsstat_,
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
  merge_statvfs(struct statvfs       * const out_,
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
  should_ignore(const StatFSIgnore::Enum  ignore_,
                const Branch             *branch_,
                const bool                readonly_)
  {
    return ((((ignore_ == StatFSIgnore::RO) || readonly_) &&
             (branch_->ro_or_nc())) ||
            ((ignore_ == StatFSIgnore::NC) && (branch_->nc())));
  }

  static
  int
  statfs(const Branches           &branches_,
         const char               *fusepath_,
         const StatFS::Enum        mode_,
         const StatFSIgnore::Enum  ignore_,
         struct statvfs           *fsstat_)
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
        fullpath = ((mode_ == StatFS::FULL) ?
                    fs::path::make(&branches_[i].path,fusepath_) :
                    branches_[i].path);

        rv = fs::lstat(fullpath,&st);
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

        if(l::should_ignore(ignore_,&branches_[i],StatVFS::readonly(stvfs)))
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
        *fsstat_ = iter->second;
        l::normalize_statvfs(fsstat_,min_bsize,min_frsize,min_namemax);

        for(++iter; iter != enditer; ++iter)
          {
            l::normalize_statvfs(&iter->second,min_bsize,min_frsize,min_namemax);
            l::merge_statvfs(fsstat_,&iter->second);
          }
      }

    return 0;
  }
}

namespace FUSE
{
  int
  statfs(const char     *fusepath_,
         struct statvfs *st_)
  {
    const fuse_context      *fc     = fuse_get_context();
    const Config            &config = Config::get(fc);
    const ugid::Set          ugid(fc->uid,fc->gid);
    const rwlock::ReadGuard  readlock(&config.branches_lock);

    return l::statfs(config.branches,
                     fusepath_,
                     config.statfs,
                     config.statfs_ignore,
                     st_);
  }
}
