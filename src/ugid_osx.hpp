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
#include <sys/unistd.h>

namespace mergerfs
{
  namespace ugid
  {
    extern __thread uid_t currentuid;
    extern __thread gid_t currentgid;
    extern __thread bool  initialized;

    static
    void
    init()
    {
      if(initialized == false)
        {
          pthread_getugid_np(&currentuid,&currentgid);
          initialized = true;
        }
    }

    static
    void
    set(const uid_t newuid,
        const gid_t newgid)
    {
      if(currentuid != newuid || currentgid != newgid)
        {
          pthread_setugid_np(KAUTH_UID_NONE,KAUTH_GID_NONE);
          pthread_setugid_np(newuid,newgid);
          currentuid = newuid;
          currentgid = newgid;
        }
    }

    struct SetReset
    {
      SetReset(const uid_t newuid,
               const gid_t newgid)
      {
        init();

        olduid = currentuid;
        oldgid = currentgid;

        set(newuid,newgid);
      }

      ~SetReset()
      {
        set(olduid,oldgid);
      }

      uid_t olduid;
      gid_t oldgid;
    };

    struct SuperUser
    {
      SuperUser()
        : setreset(0,0)
      {}

      SetReset setreset;
    };

    struct Set
    {
      Set(const uid_t newuid,
          const gid_t newgid)
      {
        init();

        set(newuid,newgid);
      }
    };
  }
}
