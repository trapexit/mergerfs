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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <grp.h>
#include <pwd.h>

#include <map>
#include <vector>

namespace mergerfs
{
  namespace ugid
  {
    extern __thread uid_t currentuid;
    extern __thread gid_t currentgid;
    extern __thread bool  initialized;

    struct Set
    {
      Set(const uid_t newuid,
          const gid_t newgid)
      {
        if(!initialized)
          {
            currentuid  = ::syscall(SYS_geteuid);
            currentgid  = ::syscall(SYS_getegid);
            initialized = true;
          }

        if(newuid == currentuid && newgid == currentgid)
          return;

        if(currentuid != 0)
          {
            ::syscall(SYS_setreuid,-1,0);
            ::syscall(SYS_setregid,-1,0);
          }

        if(newgid)
          {
            ::syscall(SYS_setregid,-1,newgid);
            initgroups(newuid,newgid);
          }

        if(newuid)
          ::syscall(SYS_setreuid,-1,newuid);

        currentuid = newuid;
        currentgid = newgid;
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
}
