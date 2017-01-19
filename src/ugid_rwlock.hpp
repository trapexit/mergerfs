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

#ifndef __UGID_RWLOCK_HPP__
#define __UGID_RWLOCK_HPP__

#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace mergerfs
{
  namespace ugid
  {
    extern uid_t currentuid;
    extern gid_t currentgid;
    extern pthread_rwlock_t rwlock;

    static
    void
    ugid_set(const uid_t newuid,
             const gid_t newgid)
    {
      pthread_rwlock_rdlock(&rwlock);

      if(newuid == currentuid && newgid == currentgid)
        return;

      pthread_rwlock_unlock(&rwlock);
      pthread_rwlock_wrlock(&rwlock);

      if(newuid == currentuid && newgid == currentgid)
        return;

      if(currentuid != 0)
        {
          ::seteuid(0);
          ::setegid(0);
        }

      if(newgid)
        {
          ::setegid(newgid);
          initgroups(newuid,newgid);
        }

      if(newuid)
        ::seteuid(newuid);

      currentuid = newuid;
      currentgid = newgid;
    }

    struct Set
    {
      Set(const uid_t newuid,
          const gid_t newgid)
      {
        ugid_set(newuid,newgid);
      }

      ~Set()
      {
        pthread_rwlock_unlock(&rwlock);
      }
    };

    struct SetRootGuard
    {
      SetRootGuard() :
        prevuid(currentuid),
        prevgid(currentgid)
      {
        pthread_rwlock_unlock(&rwlock);
        ugid_set(0,0);
      }

      ~SetRootGuard()
      {
        pthread_rwlock_unlock(&rwlock);
        ugid_set(prevuid,prevgid);
      }

      const uid_t prevuid;
      const gid_t prevgid;
    };
  }
}

#endif
