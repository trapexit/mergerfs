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

#if READ_BUF

#include <string>

#include <fuse.h>

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

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

typedef struct fuse_bufvec fuse_bufvec;

static
int
_read_buf(const int      fd,
          fuse_bufvec  **bufp,
          const size_t   size,
          const off_t    offset)
{
  int rv;
  fuse_bufvec *src;
  void *buf;

  src = (fuse_bufvec*)malloc(sizeof(fuse_bufvec));
  if(src == NULL)
    return -ENOMEM;

  *src = FUSE_BUFVEC_INIT(size);

  buf = malloc(size);
  if(buf == NULL)
    return -ENOMEM;

  rv = fs::pread(fd,buf,size,offset);
  if(rv == -1)
    return -errno;

  src->buf->mem   = buf;
  src->buf->size  = rv;

  *bufp = src;

  return 0;
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
_open_file_and_check(const std::string &fullpath,
                     const int          flags)
{
  int fd;
  int rv;

  fd = fs::open(fullpath,flags);
  if(fd == -1)
    return -1;

  rv = fs::pread(fd,&rv,1,0);
  if(rv == -1)
    return (fs::close(fd),-1);

  return fd;
}

static
int
_failover_read_buf_loop(const std::vector<std::string>  &basepaths,
                        const char                      *fusepath,
                        fuse_bufvec                    **bufp,
                        const size_t                     size,
                        const off_t                      offset,
                        FileInfo                        *fi)
{
  int fd;
  std::string fullpath;

  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      fs::path::make(&basepaths[i],fusepath,fullpath);

      fd = _open_file_and_check(fullpath,fi->flags);
      if(fd == -1)
        continue;

      fs::close(fi->fd);
      fi->fd = fd;

      return _read_buf(fd,bufp,size,offset);
    }

  return -EIO;
}

static
int
_failover_read_buf(const char      *fusepath,
                   fuse_bufvec    **bufp,
                   const size_t     size,
                   const off_t      offset,
                   FileInfo        *fi)
{
  static pthread_mutex_t   failover_lock = PTHREAD_MUTEX_INITIALIZER;

  const fuse_context      *fc            = fuse_get_context();
  const Config            &config        = Config::get(fc);
  const ugid::Set          ugid(fc->uid,fc->gid);
  const rwlock::ReadGuard  read_guard(&config.srcmountslock);
  const mutex::Guard       failover_guard(failover_lock);

  return _failover_read_buf_loop(config.srcmounts,fusepath,bufp,size,offset,fi);
}


namespace mergerfs
{
  namespace fuse
  {
    int
    read_buf(const char      *fusepath,
             fuse_bufvec    **bufp,
             size_t           size,
             off_t            offset,
             fuse_file_info  *ffi)
    {
      int rv;
      FileInfo *fi = reinterpret_cast<FileInfo*>(ffi->fh);

      rv = _read_buf(fi->fd,bufp,size,offset);
      if(_can_failover(rv,fi->flags))
        rv = _failover_read_buf(fusepath,bufp,size,offset,fi);

      return rv;
    }
  }
}

#endif
