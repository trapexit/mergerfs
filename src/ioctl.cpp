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
#include <vector>

#include <errno.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include "config.hpp"
#include "ugid.hpp"
#include "rwlock.hpp"

using std::string;
using std::vector;
using namespace mergerfs;

static
int
_ioctl(const int           fd,
       const int           cmd,
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

#ifdef FUSE_IOCTL_DIR
static
int
_ioctl_dir_base(Policy::Func::Search  searchFunc,
                const vector<string> &srcmounts,
                const size_t          minfreespace,
                const string         &fusepath,
                const int             cmd,
                void                 *arg,
                const unsigned int    flags,
                void                 *data)
{
  int fd;
  int rv;
  vector<string> path;

  rv = searchFunc(srcmounts,fusepath,minfreespace,path);
  if(rv == -1)
    return -errno;

  fs::path::append(path[0],fusepath);

  fd = ::open(path[0].c_str(),flags);
  if(fd == -1)
    return -errno;

  rv = _ioctl(fd,cmd,arg,flags,data);

  ::close(fd);

  return rv;
}

static
int
_ioctl_dir(const string       &fusepath,
           const int           cmd,
           void               *arg,
           const unsigned int  flags,
           void               *data)
{
  const struct fuse_context *fc     = fuse_get_context();
  const config::Config      &config = config::get(fc);
  const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
  const rwlock::ReadGuard    readlock(&config.srcmountslock);

  return _ioctl_dir_base(config.getattr,
                         config.srcmounts,
                         config.minfreespace,
                         fusepath,
                         cmd,
                         arg,
                         flags,
                         data);
}
#endif

namespace mergerfs
{
  namespace ioctl
  {
    int
    ioctl(const char            *fusepath,
          int                    cmd,
          void                  *arg,
          struct fuse_file_info *ffi,
          unsigned int           flags,
          void                  *data)
    {
#ifdef FUSE_IOCTL_DIR
      if(flags & FUSE_IOCTL_DIR)
        return _ioctl_dir(fusepath,
                          cmd,
                          arg,
                          flags,
                          data);
#endif

      return _ioctl(ffi->fh,
                    cmd,
                    arg,
                    flags,
                    data);
    }
  }
}
