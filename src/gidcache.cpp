/*
  The MIT License (MIT)

  Copyright (c) 2015 Antonio SJ Musumeci <trapexit@spawn.link>

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

#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>

#if defined __linux__ and UGID_USE_RWLOCK == 0
# include <sys/syscall.h>
#endif

#include <cstdlib>
#include <algorithm>

#include "gidcache.hpp"

inline
bool
gid_t_rec::operator<(const struct gid_t_rec &b) const
{
  return uid < b.uid;
}

inline
gid_t_rec *
gid_t_cache::begin(void)
{
  return recs;
}

inline
gid_t_rec *
gid_t_cache::end(void)
{
  return recs + size;
}

inline
gid_t_rec *
gid_t_cache::allocrec(void)
{
  if(size == MAXRECS)
    return &recs[rand() % MAXRECS];
  else
    return &recs[size++];
}

inline
gid_t_rec *
gid_t_cache::lower_bound(gid_t_rec   *begin,
                         gid_t_rec   *end,
                         const uid_t  uid)
{
  int step;
  int count;
  gid_t_rec *iter;

  count = std::distance(begin,end);
  while(count > 0)
    {
      iter = begin;
      step = count / 2;
      std::advance(iter,step);
      if(iter->uid < uid)
        {
          begin  = ++iter;
          count -= step + 1;
        }
      else
        {
          count = step;
        }
    }

  return begin;
}

gid_t_rec *
gid_t_cache::cache(const uid_t uid,
                   const gid_t gid)
{
  int rv;
  char buf[4096];
  struct passwd pwd;
  struct passwd *pwdrv;
  gid_t_rec *rec;

  rec = allocrec();

  rec->uid = uid;
  rv = ::getpwuid_r(uid,&pwd,buf,sizeof(buf),&pwdrv);
  if(pwdrv != NULL && rv == 0)
    {
      rec->size = 0;
      ::getgrouplist(pwd.pw_name,gid,NULL,&rec->size);
      rec->size = std::min(MAXGIDS,rec->size);
      rv = ::getgrouplist(pwd.pw_name,gid,rec->gids,&rec->size);
      if(rv == -1)
        {
          rec->gids[0] = gid;
          rec->size    = 1;
        }
    }

  return rec;
}

static
inline
int
setgroups(const gid_t_rec *rec)
{
#if defined __linux__ and UGID_USE_RWLOCK == 0
  return ::syscall(SYS_setgroups,rec->size,rec->gids);
#else
  return ::setgroups(rec->size,rec->gids);
#endif
}

int
gid_t_cache::initgroups(const uid_t uid,
                        const gid_t gid)
{
  int rv;
  gid_t_rec *rec;

  rec = lower_bound(begin(),end(),uid);
  if(rec == end() || rec->uid != uid)
    {
      rec = cache(uid,gid);
      rv = ::setgroups(rec);
      std::sort(begin(),end());
    }
  else
    {
      rv = ::setgroups(rec);
    }

  return rv;
}
