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

#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <grp.h>
#include <pwd.h>

#include <map>
#include <vector>

#if defined SYS_setreuid32
#define SETREUID(R,E) (::syscall(SYS_setreuid32,(R),(E)))
#else
#define SETREUID(R,E) (::syscall(SYS_setreuid,(R),(E)))
#endif

#if defined SYS_setregid32
#define SETREGID(R,E) (::syscall(SYS_setregid32,(R),(E)))
#else
#define SETREGID(R,E) (::syscall(SYS_setregid,(R),(E)))
#endif

#if defined SYS_geteuid32
#define GETEUID() (::syscall(SYS_geteuid32))
#else
#define GETEUID() (::syscall(SYS_geteuid))
#endif

#if defined SYS_getegid32
#define GETEGID() (::syscall(SYS_getegid32))
#else
#define GETEGID() (::syscall(SYS_getegid))
#endif

namespace ugid
{
  extern __thread uid_t currentuid;
  extern __thread gid_t currentgid;
  extern __thread bool  initialized;

  struct Set
  {
    Set(const uid_t newuid_,
        const gid_t newgid_)
    {
      if(!initialized)
        {
          currentuid  = GETEUID();
          currentgid  = GETEGID();
          initialized = true;
        }

      if((newuid_ == currentuid) && (newgid_ == currentgid))
        return;

      if(currentuid != 0)
        {
          SETREUID(-1,0);
          SETREGID(-1,0);
        }

      if(newgid_)
        {
          SETREGID(-1,newgid_);
          initgroups(newuid_,newgid_);
        }

      if(newuid_)
        SETREUID(-1,newuid_);

      currentuid = newuid_;
      currentgid = newgid_;
    }
  };

  struct SetRootGuard
  {
    SetRootGuard() :
      prevuid(currentuid),
      prevgid(currentgid)
    {
      Set(0,0);
    }

    ~SetRootGuard()
    {
      Set(prevuid,prevgid);
    }

    const uid_t prevuid;
    const gid_t prevgid;
  };
}

#undef SETREUID
#undef SETREGID
#undef GETEUID
#undef GETEGID
