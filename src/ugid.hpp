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

#ifndef __UGID_HPP__
#define __UGID_HPP__

#include <fuse.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mergerfs
{
  namespace ugid
  {
    struct SetResetGuard
    {
      SetResetGuard()
      {
        const struct fuse_context *fc = fuse_get_context();

        olduid   = ::getuid();
        oldgid   = ::getgid();
        oldumask = ::umask(fc->umask);
        newuid   = fc->uid;
        newgid   = fc->gid;
        newumask = fc->umask;

        if(olduid != newuid)
          ::seteuid(newuid);
        if(oldgid != newgid)
          ::setegid(newgid);
      }

      SetResetGuard(uid_t  u,
                    gid_t  g,
                    mode_t m)
      {
        olduid   = ::getuid();
        oldgid   = ::getgid();
        oldumask = ::umask(m);
        newuid   = u;
        newgid   = g;
        newumask = m;

        if(olduid != newuid)
          ::seteuid(newuid);
        if(oldgid != newgid)
          ::setegid(newgid);
      }

      ~SetResetGuard()
      {
        ::umask(oldumask);
        if(olduid != newuid)
          ::seteuid(olduid);
        if(oldgid != newgid)
          ::setegid(oldgid);
      }

      uid_t  olduid;
      gid_t  oldgid;
      mode_t oldumask;
      uid_t  newuid;
      gid_t  newgid;
      mode_t newumask;
    };
  }
}


#endif /* __UGID_HPP__ */
