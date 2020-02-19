#include "fuse_dev.h"

#include <stdlib.h>
#include <sys/uio.h>
#include <unistd.h>

fuse_dev_t*
fuse_dev_alloc(void)
{
  return calloc(1,sizeof(fuse_dev_t));
}

void
fuse_dev_free(fuse_dev_t *fd_)
{
  if(fd_)
    free(fd_);
}

fuse_dev_t*
fuse_dev_clone(fuse_dev_t *fd_)
{
  return fd_;
}

int64_t
fuse_dev_read(const fuse_dev_t *fd_,
              void             *buf_,
              uint64_t          count_)
{
  int64_t rv;

  rv = read(fd_->fd,buf_,count_);

  return rv;
}

int64_t
fuse_dev_write(const fuse_dev_t   *fd_,
               const struct iovec *iov_,
               int                 iovcnt_)
{
  int64_t rv;

  rv = writev(fd_->fd,iov_,iovcnt_);

  return rv;
}

int64_t
fuse_dev_write_splice(const fuse_dev_t*   fd_,
                      const struct iovec *iov_,
                      int                 iovcnt_)
{
  
}
