#define _GNU_SOURCE

#include "fuse_chan.h"
#include "fuse_lowlevel.h"
#include "fuse_kernel.h"
#include "sys.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static const char DEV_FUSE[] = "/dev/fuse";

static
void*
aligned_mem(const uint64_t size_)
{
  int rv;
  void *mem;
  int pagesize;

  pagesize = sys_get_pagesize();

  rv = posix_memalign(&mem,pagesize,size_);
  if(rv == 0)
    return mem;

  errno = rv;
  return NULL;
}

static
int
clone_devfuse_fd(const int devfuse_fd_)
{
#ifdef FUSE_DEV_IOC_CLONE
  int rv;
  int clone_fd;

  clone_fd = open(DEV_FUSE,O_RDWR|O_CLOEXEC);
  if(clone_fd == -1)
    return devfuse_fd_;

  rv = ioctl(clone_fd,FUSE_DEV_IOC_CLONE,&devfuse_fd_);
  if(rv == -1)
    {
      perror("fuse: failed to clone /dev/fuse");
      close(clone_fd);
      return devfuse_fd_;
    }

  return clone_fd;
#else
  return devfuse_fd_;
#endif
}

int
fuse_msg_bufsize(void)
{
  int bufsize;

  bufsize = ((FUSE_MSG_MAX_PAGES + 1) * sys_get_pagesize());

  return bufsize;
}

fuse_chan_t*
fuse_chan_new(int32_t  devfuse_fd_,
              uint32_t flags_)
{
  int rv;
  int bufsize;
  fuse_chan_t *ch;

  ch = (fuse_chan_t*)calloc(1,sizeof(fuse_chan_t));
  if(ch == NULL)
    {
      fprintf(stderr, "fuse: failed to allocate channel memory\n");
      return NULL;
    }

  bufsize     = fuse_msg_bufsize();
  ch->fd      = clone_devfuse_fd(devfuse_fd_);
  ch->buf     = aligned_mem(bufsize);
  ch->bufsize = bufsize;
  ch->flags   = flags_;
  if(flags_ & (FUSE_CHAN_SPLICE_READ|FUSE_CHAN_SPLICE_WRITE))
    {
      rv = sys_alloc_pipe(ch->splice_pipe,bufsize);
      if(rv == -1)
        ch->flags &= ~(FUSE_CHAN_SPLICE_READ|FUSE_CHAN_SPLICE_WRITE);
    }

  return ch;
}

static
int64_t
fuse_chan_recv_splice(fuse_chan_t *ch_,
                      char        *buf_,
                      uint64_t     size_)
{
  int64_t rv;
  struct iovec iov;

 restart_splice:
  rv = splice(ch_->fd,NULL,ch_->splice_pipe[1],NULL,size_,SPLICE_F_MOVE);
  switch((rv == -1) ? errno : 0)
    {
    case 0:
      break;
    case ENOENT:
    case EINTR:
    case EAGAIN:
      goto restart_splice;
    case ENODEV:
      return 0;
    default:
      return -errno;
    }

  iov.iov_base = buf_;
  iov.iov_len  = rv;
 restart_vmsplice:
  rv = vmsplice(ch_->splice_pipe[0],&iov,1,SPLICE_F_MOVE);
  switch((rv == -1) ? errno : 0)
    {
    case 0:
      break;
    case ENOENT:
    case EINTR:
    case EAGAIN:
      goto restart_vmsplice;
    case ENODEV:
      return 0;
    default:
      return -errno;
    }

  return rv;
}

static
int
fuse_chan_recv_read(fuse_chan_t *ch_,
                    char        *buf_,
                    uint64_t     size_)
{
  int64_t rv;

 restart:
  rv = read(ch_->fd,buf_,size_);
  switch((rv == -1) ? errno : 0)
    {
    case 0:
      break;
    case ENOENT:
    case EINTR:
    case EAGAIN:
      goto restart;
    case ENODEV:
      return 0;
    default:
      return -errno;
    }

  if(rv < sizeof(struct fuse_in_header))
    {
      fprintf(stderr, "fuse: short read on fuse device\n");
      return -EIO;
    }

  return rv;
}

int64_t
fuse_chan_recv(fuse_chan_t *ch_,
               char        *buf_,
               size_t       size_)
{
  if(ch_->flags & FUSE_CHAN_SPLICE_READ)
    return fuse_chan_recv_splice(ch_,buf_,size_);
  return fuse_chan_recv_read(ch_,buf_,size_);
}

static
int64_t
fuse_chan_send_write(fuse_chan_t        *ch_,
                     const struct iovec  iov_[],
                     size_t              count_)
{
  int64_t rv;

  if(iov_ == NULL)
    return 0;

  rv = writev(ch_->fd,iov_,count_);
  if(rv == -1)
    return -errno;

  return rv;
}

static
int64_t
fuse_chan_send_splice(fuse_chan_t        *ch_,
                      const struct iovec  iov_[],
                      size_t              count_)
{
  int64_t rv;

  rv = vmsplice(ch_->splice_pipe[1],iov_,count_,SPLICE_F_MOVE);
  if(rv == -1)
    return -errno;

  rv = splice(ch_->splice_pipe[0],NULL,ch_->fd,NULL,rv,SPLICE_F_MOVE);

  return rv;
}

int64_t
fuse_chan_send(fuse_chan_t        *ch,
               const struct iovec  iov[],
               size_t              count)
{
  if(ch->flags & FUSE_CHAN_SPLICE_WRITE)
    return fuse_chan_send_splice(ch,iov,count);
  return fuse_chan_send_write(ch,iov,count);
}

void
fuse_chan_destroy(fuse_chan_t *ch)
{
  close(ch->fd);
  if(ch->flags & (FUSE_CHAN_SPLICE_READ|FUSE_CHAN_SPLICE_WRITE))
    {
      close(ch->splice_pipe[0]);
      close(ch->splice_pipe[1]);
    }
  free(ch->buf);
  free(ch);
}
