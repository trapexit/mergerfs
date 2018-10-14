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

#include <fuse.h>

#include <algorithm>
#include <climits>
#include <map>
#include <string>
#include <vector>

#include "config.hpp"
#include "errno.hpp"
#include "fs_base_stat.hpp"
#include "fs_base_statvfs.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::map;
using std::vector;

static
void
_normalize_statvfs(struct statvfs      *fsstat,
                   const unsigned long  min_bsize,
                   const unsigned long  min_frsize,
                   const unsigned long  min_namemax)
{
  fsstat->f_blocks  = (fsblkcnt_t)((fsstat->f_blocks * fsstat->f_frsize) / min_frsize);
  fsstat->f_bfree   = (fsblkcnt_t)((fsstat->f_bfree * fsstat->f_frsize) / min_frsize);
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
void
_statfs_core(const char                *srcmount,
             unsigned long             &min_bsize,
             unsigned long             &min_frsize,
             unsigned long             &min_namemax,
             map<dev_t,struct statvfs> &fsstats)
{
  int rv;
  struct stat st;
  struct statvfs fsstat;

  rv = fs::statvfs(srcmount,fsstat);
  if(rv == -1)
    return;

  rv = fs::stat(srcmount,st);
  if(rv == -1)
    return;

  if(fsstat.f_bsize   && (min_bsize > fsstat.f_bsize))
    min_bsize = fsstat.f_bsize;
  if(fsstat.f_frsize  && (min_frsize > fsstat.f_frsize))
    min_frsize = fsstat.f_frsize;
  if(fsstat.f_namemax && (min_namemax > fsstat.f_namemax))
    min_namemax = fsstat.f_namemax;

  fsstats.insert(std::make_pair(st.st_dev,fsstat));
}

static
int
_statfs(const vector<string> &srcmounts,
        struct statvfs       &fsstat)
{
  map<dev_t,struct statvfs> fsstats;
  unsigned long min_bsize   = ULONG_MAX;
  unsigned long min_frsize  = ULONG_MAX;
  unsigned long min_namemax = ULONG_MAX;

  for(size_t i = 0, ei = srcmounts.size(); i < ei; i++)
    {
      _statfs_core(srcmounts[i].c_str(),min_bsize,min_frsize,min_namemax,fsstats);
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
      const rwlock::ReadGuard  readlock(&config.srcmountslock);

      return _statfs(config.srcmounts,
                     *stat);
    }
  }
}
