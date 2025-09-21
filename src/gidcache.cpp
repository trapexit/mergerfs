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

#include "gidcache.hpp"

#include "syslog.hpp"

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

int GIDCache::expire_timeout = (60 * 60);
int GIDCache::remove_timeout = (60 * 60 * 12);
boost::concurrent_flat_map<uid_t,GIDRecord> GIDCache::_records;

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

static
inline
int
_setgroups(const std::vector<gid_t> gids_)
{
#if defined __linux__ and UGID_USE_RWLOCK == 0
# if defined SYS_setgroups32
  return ::syscall(SYS_setgroups32,gids_.size(),gids_.data());
# else
  return ::syscall(SYS_setgroups,gids_.size(),gids_.data());
# endif
#else
  return ::setgroups(gids_.size(),gids_.data());
#endif
}

static
void
_getgroups(const uid_t         uid_,
           const gid_t         gid_,
           std::vector<gid_t> &gids_)
{
  int rv;
  int ngroups;
  char buf[4096];
  struct passwd pwd;
  struct passwd *pwdrv;

  gids_.clear();
  rv = ::getpwuid_r(uid_,&pwd,buf,sizeof(buf),&pwdrv);
  if((rv == -1) || (pwdrv == NULL))
    goto error;

  ngroups = 0;
  rv = ::_getgrouplist(pwd.pw_name,gid_,NULL,&ngroups);
  gids_.resize(ngroups);

  rv = ::_getgrouplist(pwd.pw_name,gid_,gids_.data(),&ngroups);
  if((size_t)ngroups < gids_.size())
    gids_.resize(ngroups);

  return;

 error:
  gids_.clear();
  gids_.push_back(gid_);
}


int
GIDCache::initgroups(const uid_t uid_,
                     const gid_t gid_)
{
  auto first_func =
    [=](auto &x)
    {
      x.second.last_update = ::time(NULL);
      ::_getgroups(uid_,gid_,x.second.gids);
      ::_setgroups(x.second.gids);
    };
  auto exists_func =
    [=](auto &x)
    {
      time_t now;

      now = ::time(NULL);
      if((now - x.second.last_update) > GIDCache::expire_timeout)
        {
          ::_getgroups(uid_,gid_,x.second.gids);
          x.second.last_update = now;
        }
      ::_setgroups(x.second.gids);
    };

  _records.try_emplace_and_visit(uid_,
                                 first_func,
                                 exists_func);

  return 0;
}

void
GIDCache::invalidate_all()
{
  size_t size;

  size = _records.size();
  _records.visit_all([](auto &x)
  {
    x.second.last_update = 0;
  });

  SysLog::info("gid cache invalidated, {} entries",size);
}

void
GIDCache::clear_all()
{
  size_t size;

  size = _records.size();
  _records.clear();

  SysLog::info("gid cache cleared, {} entries",size);
}

void
GIDCache::clear_unused()
{
  int erased = 0;
  time_t now = ::time(NULL);
  auto erase_func =
    [now,&erased](auto &x)
    {
      bool should_erase;
      time_t time_delta;

      time_delta = (now - x.second.last_update);
      should_erase = (time_delta > GIDCache::remove_timeout);
      erased += should_erase;

      return should_erase;
    };

  _records.erase_if(erase_func);

  SysLog::info("cleared {} unused gid cache entries",erased);
}
