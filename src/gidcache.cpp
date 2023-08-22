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

#include "rnd.hpp"

#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#if defined __linux__ and UGID_USE_RWLOCK == 0
# include <sys/syscall.h>
#elif __APPLE__
# include <sys/param.h>
#endif

#include <cassert>
#include <cstdlib>
#include <algorithm>
#include <unordered_map>
#include <mutex>

#include "gidcache.hpp"

std::mutex                              g_REGISTERED_CACHES_MUTEX;
std::unordered_map<pthread_t,GIDCache*> g_REGISTERED_CACHES;


inline
bool
GIDRecord::operator<(const struct GIDRecord &b) const
{
  return uid < b.uid;
}

GIDCache::GIDCache()
  : invalidate(false),
    size(0),
    recs()
{
  std::lock_guard<std::mutex> guard(g_REGISTERED_CACHES_MUTEX);

  bool inserted;
  pthread_t pthid;

  pthid = pthread_self();
  inserted = g_REGISTERED_CACHES.emplace(pthid,this).second;

  assert(inserted == true);
}

inline
GIDRecord *
GIDCache::begin(void)
{
  return &recs[0];
}

inline
GIDRecord *
GIDCache::end(void)
{
  return &recs[size];
}

inline
GIDRecord *
GIDCache::allocrec(void)
{
  if(size == MAXRECS)
    return &recs[RND::rand64(MAXRECS)];
  else
    return &recs[size++];
}

inline
GIDRecord *
GIDCache::lower_bound(GIDRecord   *begin,
                      GIDRecord   *end,
                      const uid_t  uid)
{
  int step;
  int count;
  GIDRecord *iter;

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

static
int
_getgrouplist(const char  *user,
              const gid_t  group,
              gid_t       *groups,
              int         *ngroups)
{
#if __APPLE__
  return ::getgrouplist(user,group,(int*)groups,ngroups);
#else
  return ::getgrouplist(user,group,groups,ngroups);
#endif
}

GIDRecord *
GIDCache::cache(const uid_t uid,
                const gid_t gid)
{
  int rv;
  char buf[4096];
  struct passwd pwd;
  struct passwd *pwdrv;
  GIDRecord *rec;

  rec = allocrec();

  rec->uid = uid;
  rv = ::getpwuid_r(uid,&pwd,buf,sizeof(buf),&pwdrv);
  if(pwdrv != NULL && rv == 0)
    {
      rec->size = 0;
      ::_getgrouplist(pwd.pw_name,gid,NULL,&rec->size);
      rec->size = std::min(MAXGIDS,rec->size);
      rv = ::_getgrouplist(pwd.pw_name,gid,rec->gids,&rec->size);
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
setgroups(const GIDRecord *rec)
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
GIDCache::initgroups(const uid_t uid,
                     const gid_t gid)
{
  int rv;
  GIDRecord *rec;

  if(invalidate)
    {
      size       = 0;
      invalidate = false;
    }

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

void
GIDCache::invalidate_all_caches()
{
  std::lock_guard<std::mutex> guard(g_REGISTERED_CACHES_MUTEX);

  for(auto &p : g_REGISTERED_CACHES)
    p.second->invalidate = true;
}
