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

#include <fuse.h>

#include <string>

#include <errno.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include "config.hpp"
#include "fileinfo.hpp"
#include "ugid.hpp"

static
int
_ioctl(const int           fd,
       const long          cmd,
       void               *arg,
       const unsigned int  flags,
       void               *data)
{
  int rv;

  switch(cmd)
    {
#ifdef FS_IOC_GETFLAGS
    case FS_IOC_GETFLAGS:
    case FS_IOC_SETFLAGS:
#endif
#ifdef FS_IOC32_GETFLAGS
    case FS_IOC32_SETFLAGS:
    case FS_IOC32_GETFLAGS:
#endif
#ifdef FS_IOC_GETVERSION
    case FS_IOC_GETVERSION:
    case FS_IOC_SETVERSION:
#endif
#ifdef FS_IOC32_GETVERSION
    case FS_IOC32_GETVERSION:
    case FS_IOC32_SETVERSION:
#endif
      rv = ::ioctl(fd,cmd,data);
      break;

    default:
      rv = -1;
      errno = ENOTTY;
      break;
    }

  return ((rv == -1) ? -errno : rv);
}

namespace mergerfs
{
  namespace ioctl
  {
    int
    ioctl(const char            *fusepath,
          int                    cmd,
          void                  *arg,
          struct fuse_file_info *fi,
          unsigned int           flags,
          void                  *data)
    {
      const ugid::SetResetGuard  ugid;
      const config::Config      &config   = config::get();
      const FileInfo            *fileinfo = (FileInfo*)fi->fh;

      if(fusepath == config.controlfile)
        return -EINVAL;

      return _ioctl(fileinfo->fd,
                    (long)cmd,
                    arg,
                    flags,
                    data);
    }
  }
}
