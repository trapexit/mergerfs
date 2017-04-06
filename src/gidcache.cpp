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

#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/types.h>
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
  long step;
  long count;
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
 #if __APPLE__
      // OSX:   getgrouplist(const char *name, int basegid, int *groups, int *ngroups)
      rec->size = 0;
      ::getgrouplist(pwd.pw_name,(int)gid,NULL,&rec->size);
      rec->size = std::min(MAXGIDS,rec->size);
      
      rv = ::getgrouplist(pwd.pw_name,(int)gid,(int*)rec->gids,&rec->size);
#else
      // Linux: getgrouplist(const char *name, gid_t group, gid_t *groups int *ngroups)
      rec->size = 0;
      ::getgrouplist(pwd.pw_name,gid,NULL,&rec->size);
      rec->size = std::min(MAXGIDS,rec->size);
      
      rv = ::getgrouplist(pwd.pw_name,gid,rec->gids,&rec->size);
#endif
      
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
# if defined SYS_setgroups32
  return ::syscall(SYS_setgroups32,rec->size,rec->gids);
# else
  return ::syscall(SYS_setgroups,rec->size,rec->gids);
# endif
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
