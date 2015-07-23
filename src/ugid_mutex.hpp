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
#include <pthread.h>

namespace mergerfs
{
  namespace ugid
  {
    extern uid_t currentuid;
    extern gid_t currentgid;
    extern bool  initialized;
    extern pthread_mutex_t lock;

    static
    void
    init()
    {
      if(initialized == false)
        {
          currentuid  = ::geteuid();
          currentgid  = ::getegid();
          initialized = true;
        }
    }

    static
    void
    set(const uid_t newuid,
        const gid_t newgid)
    {

    }

    struct SetReset
    {
      SetReset(const uid_t _newuid,
               const gid_t _newgid)
      {
        init();

        olduid = currentuid;
        oldgid = currentgid;

        seteuid(0);
        if(_newgid != oldgid)
          setegid(newgid);
        if(_newuid != olduid)
          seteuid(newuid);
      }

      ~SetResetGuard()
      {
        if(olduid != newuid)
          seteuid(olduid);
        if(oldgid != newgid)
          setegid(oldgid);
      }

      uid_t  olduid;
      gid_t  oldgid;
      uid_t  newuid;
      gid_t  newgid;
    };

    struct SuperUser : public SetReset
    {
      SuperUser()
        : UnlockedSetReset(0,0)
      {}
    };

  }
}
