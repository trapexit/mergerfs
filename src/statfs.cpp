/*
   The MIT License (MIT)

   Copyright (c) 2014 Antonio SJ Musumeci <trapexit@spawn.link>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include <fuse.h>

#include <sys/statvfs.h>
#include <errno.h>

#include <climits>
#include <string>
#include <vector>
#include <map>

#include "ugid.hpp"
#include "config.hpp"
#include "rwlock.hpp"

using std::string;
using std::vector;
using std::map;
using std::pair;

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
int
_statfs(const vector<string> &srcmounts,
        struct statvfs       &fsstat)
{
  unsigned long min_bsize   = ULONG_MAX;
  unsigned long min_frsize  = ULONG_MAX;
  unsigned long min_namemax = ULONG_MAX;
  map<unsigned long,struct statvfs> fsstats;
  vector<string>::const_iterator iter;
  vector<string>::const_iterator enditer;

  for(iter = srcmounts.begin(), enditer = srcmounts.end(); iter != enditer; ++iter)
    {
      int rv;
      struct statvfs fsstat;
      rv = ::statvfs(iter->c_str(),&fsstat);
      if(rv != 0)
        continue;

      if(min_bsize > fsstat.f_bsize)
        min_bsize = fsstat.f_bsize;
      if(min_frsize > fsstat.f_frsize)
        min_frsize = fsstat.f_frsize;
      if(min_namemax > fsstat.f_namemax)
        min_namemax = fsstat.f_namemax;

      fsstats.insert(pair<unsigned long,struct statvfs>(fsstat.f_fsid,fsstat));
    }

  map<unsigned long,struct statvfs>::iterator fsstatiter    = fsstats.begin();
  map<unsigned long,struct statvfs>::iterator endfsstatiter = fsstats.end();
  if(fsstatiter != endfsstatiter)
    {
      fsstat = fsstatiter->second;
      _normalize_statvfs(&fsstat,min_bsize,min_frsize,min_namemax);
      for(++fsstatiter;fsstatiter != endfsstatiter;++fsstatiter)
        {
          _normalize_statvfs(&fsstatiter->second,min_bsize,min_frsize,min_namemax);
          _merge_statvfs(&fsstat,&fsstatiter->second);
        }
    }

  return 0;
}

namespace mergerfs
{
  namespace statfs
  {
    int
    statfs(const char     *fusepath,
           struct statvfs *stat)
    {
      const struct fuse_context *fc     = fuse_get_context();
      const config::Config      &config = config::get();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard    readlock(&config.srcmountslock);

      return _statfs(config.srcmounts,
                     *stat);
    }
  }
}
