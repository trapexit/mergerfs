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

#include <string.h>
#include <pthread.h>

#include <string>

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_base_close.hpp"
#include "fs_base_open.hpp"
#include "fs_base_read.hpp"
#include "fs_path.hpp"
#include "mutex.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using namespace mergerfs;

static
int
_read(const int     fd,
      void         *buf,
      const size_t  count,
      const off_t   offset)
{
  int rv;

  rv = fs::pread(fd,buf,count,offset);

  return ((rv == -1) ? -errno : rv);
}

static
bool
_can_failover(const int error,
              const int flags)
{
  return (((error == -EIO) ||
           (error == -ENOTCONN)) &&
          !(flags & (O_CREAT|O_TRUNC)));
}

static
int
_failover_read_loop(const std::vector<std::string> &basepaths,
                    const char                     *fusepath,
                    char                           *buf,
                    const size_t                    count,
                    const off_t                     offset,
                    FileInfo                       *fi,
                    const int                       error)
{
  int fd;
  int rv;
  std::string fullpath;

  rv = fs::pread(fi->fd,buf,count,offset);
  if(rv >= 0)
    return rv;

  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      fs::path::make(basepaths[i],fusepath,fullpath);

      fd = fs::open(fullpath,fi->flags);
      if(fd == -1)
        continue;

      rv = fs::pread(fd,buf,count,offset);
      if(rv >= 0)
        {
          fs::close(fi->fd);
          fi->fd = fd;
          return rv;
        }

      fs::close(fd);
    }

  return error;
}

static
int
_failover_read(const char   *fusepath,
               char         *buf,
               const size_t  count,
               const off_t   offset,
               FileInfo     *fi,
               const int     error)
{
  static pthread_mutex_t   failover_lock = PTHREAD_MUTEX_INITIALIZER;

  const fuse_context      *fc            = fuse_get_context();
  const Config            &config        = Config::get(fc);
  const ugid::Set          ugid(fc->uid,fc->gid);
  const rwlock::ReadGuard  read_guard(&config.srcmountslock);
  const mutex::Guard       failover_guard(failover_lock);

  return _failover_read_loop(config.srcmounts,fusepath,buf,count,offset,fi,error);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    read(const char     *fusepath,
         char           *buf,
         size_t          count,
         off_t           offset,
         fuse_file_info *ffi)
    {
      int rv;
      FileInfo *fi = reinterpret_cast<FileInfo*>(ffi->fh);

      rv = _read(fi->fd,buf,count,offset);
      if(_can_failover(rv,fi->flags))
        rv = _failover_read(fusepath,buf,count,offset,fi,rv);

      return rv;
    }
  }
}
