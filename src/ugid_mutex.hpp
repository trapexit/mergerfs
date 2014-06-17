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
    struct SetResetGuard
    {
      SetResetGuard(const uid_t _newuid,
                    const gid_t _newgid)
      {
        pthread_mutex_lock(&lock);

        olduid   = ::geteuid();
        oldgid   = ::getegid();
        newuid   = _newuid;
        newgid   = _newgid;

        if(newgid != oldgid)
          setegid(newgid);
        if(newuid != olduid)
          seteuid(newuid);
      }

      ~SetResetGuard()
      {
        if(olduid != newuid)
          seteuid(newuid);
        if(oldgid != newgid)
          setegid(newgid);
      }

      uid_t  olduid;
      gid_t  oldgid;
      uid_t  newuid;
      gid_t  newgid;

      static pthread_mutex_t lock;
    };
  }
}
