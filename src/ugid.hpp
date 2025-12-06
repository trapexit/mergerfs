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

#include "fuse_req_ctx.h"
#include "predictability.h"

#include "fuse_kernel.h"

#include <assert.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>


struct ugid_t
{
  ugid_t(const uid_t uid_,
         const gid_t gid_)
    : uid(uid_),
      gid(gid_)
  {
  }

  ugid_t(const fuse_req_ctx_t *ctx_)
    : ugid_t(ctx_->uid,ctx_->gid)
  {
  }

  uid_t uid;
  gid_t gid;
};

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
  extern thread_local uid_t currentuid;
  extern thread_local gid_t currentgid;
  extern thread_local bool  initialized;

  static
  inline
  void
  set(const uid_t newuid_,
      const gid_t newgid_)
  {
    assert(newuid_ != FUSE_INVALID_UIDGID);
    assert(newgid_ != FUSE_INVALID_UIDGID);

    if(unlikely(!initialized))
      {
        currentuid  = GETEUID();
        currentgid  = GETEGID();
        initialized = true;
      }

    if((newuid_ == currentuid) && (newgid_ == currentgid))
      return;

    SETREGID(-1,newgid_);
    SETREUID(-1,newuid_);

    currentuid = newuid_;
    currentgid = newgid_;
  }

  struct SetGuard
  {
    SetGuard(const ugid_t ugid_)
      : prev(currentuid,currentgid)
    {
      assert(currentuid == 0);
      assert(currentgid == 0);
      ugid::set(ugid_.uid,ugid_.gid);
    }

    ~SetGuard()
    {
      ugid::set(prev.uid,prev.gid);
    }

    const ugid_t prev;
  };
}

#undef SETREUID
#undef SETREGID
#undef GETEUID
#undef GETEGID
