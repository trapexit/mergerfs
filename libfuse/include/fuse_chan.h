#pragma once

#include <stdint.h>
#include <sys/uio.h>

#define FUSE_CHAN_SPLICE_READ  (1<<0)
#define FUSE_CHAN_SPLICE_WRITE (1<<1)

typedef struct fuse_chan_t fuse_chan_t;
struct fuse_chan_t
{
  void     *buf;
  uint64_t  bufsize;
  int32_t   fd;
  int32_t   splice_pipe[2];
  uint32_t  flags;
};

fuse_chan_t *fuse_chan_new(int32_t fd, uint32_t flags);
void         fuse_chan_destroy(fuse_chan_t *ch);
int64_t      fuse_chan_recv(fuse_chan_t *ch, char *buf, uint64_t size);
int64_t      fuse_chan_send(fuse_chan_t *ch, const struct iovec iov[], uint64_t count);

