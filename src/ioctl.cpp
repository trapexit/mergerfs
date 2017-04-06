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

#include <fuse.h>

#include <string>
#include <vector>

#include <fcntl.h>

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_base_close.hpp"
#include "fs_base_ioctl.hpp"
#include "fs_base_open.hpp"
#include "fs_path.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using namespace mergerfs;

static
int
_ioctl(const int            fd,
       const unsigned long  cmd,
       void                *data)
{
  int rv;

  rv = fs::ioctl(fd,cmd,data);

  return ((rv == -1) ? -errno : rv);
}

#ifdef FUSE_IOCTL_DIR

#ifndef O_NOATIME
#define O_NOATIME 0
#endif

static
int
_ioctl_dir_base(Policy::Func::Search  searchFunc,
                const vector<string> &srcmounts,
                const uint64_t        minfreespace,
                const char           *fusepath,
                const unsigned long   cmd,
                void                 *data)
{
  int fd;
  int rv;
  string fullpath;
  vector<const string*> basepaths;

  rv = searchFunc(srcmounts,fusepath,minfreespace,basepaths);
  if(rv == -1)
    return -errno;

  fs::path::make(basepaths[0],fusepath,fullpath);

  const int flags = O_RDONLY | O_NOATIME | O_NONBLOCK;
  fd = fs::open(fullpath,flags);
  if(fd == -1)
    return -errno;

  rv = _ioctl(fd,cmd,data);

  fs::close(fd);

  return rv;
}

static
int
_ioctl_dir(const char         *fusepath,
           const unsigned long cmd,
           void               *data)
{
  const fuse_context      *fc     = fuse_get_context();
  const Config            &config = Config::get(fc);
  const ugid::Set          ugid(fc->uid,fc->gid);
  const rwlock::ReadGuard  readlock(&config.srcmountslock);

  return _ioctl_dir_base(config.getattr,
                         config.srcmounts,
                         config.minfreespace,
                         fusepath,
                         cmd,
                         data);
}
#endif

namespace mergerfs
{
  namespace fuse
  {
    int
    ioctl(const char     *fusepath,
          int             cmd,
          void           *arg,
          fuse_file_info *ffi,
          unsigned int    flags,
          void           *data)
    {
#ifdef FUSE_IOCTL_DIR
      if(flags & FUSE_IOCTL_DIR)
        return _ioctl_dir(fusepath,
           (unsigned long)cmd,
                          data);
#endif
      FileInfo *fi = reinterpret_cast<FileInfo*>(ffi->fh);

      return _ioctl(fi->fd,
     (unsigned long)cmd,
                    data);
    }
  }
}
