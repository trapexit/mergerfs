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

#pragma once

#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace ugid
{
  extern uid_t currentuid;
  extern gid_t currentgid;
  extern pthread_rwlock_t rwlock;

  static
  void
  ugid_set(const uid_t newuid_,
           const gid_t newgid_)
  {
    pthread_rwlock_rdlock(&rwlock);

    if((newuid_ == currentuid) && (newgid_ == currentgid))
      return;

    pthread_rwlock_unlock(&rwlock);
    pthread_rwlock_wrlock(&rwlock);

    if((newuid_ == currentuid) && (newgid_ == currentgid))
      return;

    if(currentuid != 0)
      {
        ::seteuid(0);
        ::setegid(0);
      }

    if(newgid_)
      {
        ::setegid(newgid_);
        initgroups(newuid_,newgid_);
      }

    if(newuid_)
      ::seteuid(newuid_);

    currentuid = newuid_;
    currentgid = newgid_;
  }

  struct Set
  {
    Set(const uid_t newuid_,
        const gid_t newgid_)
    {
      ugid_set(newuid_,newgid_);
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
