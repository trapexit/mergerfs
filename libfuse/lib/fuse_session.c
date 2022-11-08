/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#include "fuse_i.h"
#include "fuse_misc.h"
#include "fuse_kernel.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>


struct fuse_chan
{
  struct fuse_session *se;
  int fd;
  size_t bufsize;
};

struct fuse_session *fuse_session_new(void *data,
                                      void *receive_buf,
                                      void *process_buf,
                                      void *destroy)
{
  struct fuse_session *se = (struct fuse_session *) malloc(sizeof(*se));
  if (se == NULL) {
    fprintf(stderr, "fuse: failed to allocate session\n");
    return NULL;
  }

  memset(se, 0, sizeof(*se));
  se->data = data;
  se->receive_buf = receive_buf;
  se->process_buf = process_buf;
  se->destroy     = destroy;

  return se;
}

void fuse_session_add_chan(struct fuse_session *se, struct fuse_chan *ch)
{
  assert(se->ch == NULL);
  assert(ch->se == NULL);
  se->ch = ch;
  ch->se = se;
}

void fuse_session_remove_chan(struct fuse_chan *ch)
{
  struct fuse_session *se = ch->se;
  if (se) {
    assert(se->ch == ch);
    se->ch = NULL;
    ch->se = NULL;
  }
}

void
fuse_session_destroy(struct fuse_session *se)
{
  se->destroy(se->data);
  if(se->ch != NULL)
    fuse_chan_destroy(se->ch);
  free(se);
}

void fuse_session_reset(struct fuse_session *se)
{
  se->exited = 0;
}

int
fuse_session_exited(struct fuse_session *se)
{
  return se->exited;
}

void
fuse_session_exit(struct fuse_session *se_)
{
  se_->exited = 1;
}

void *fuse_session_data(struct fuse_session *se)
{
  return se->data;
}

int
fuse_session_receive(struct fuse_session *se_,
                     struct fuse_buf     *buf_)
{
  return se_->receive_buf(se_,buf_,se_->ch);
}

void
fuse_session_process(struct fuse_session   *se_,
                     const struct fuse_buf *buf_)
{
  se_->process_buf(se_->data,buf_,se_->ch);
}

struct fuse_chan *
fuse_chan_new(int fd,
              size_t bufsize)
{
  struct fuse_chan *ch;

  ch = (struct fuse_chan*)malloc(sizeof(*ch));
  if(ch == NULL)
    {
      fprintf(stderr, "fuse: failed to allocate channel\n");
      return NULL;
    }

  memset(ch, 0, sizeof(*ch));

  ch->fd = fd;
  ch->bufsize = bufsize;

  return ch;
}

int fuse_chan_fd(struct fuse_chan *ch)
{
  return ch->fd;
}

int fuse_chan_clearfd(struct fuse_chan *ch)
{
  int fd = ch->fd;
  ch->fd = -1;
  return fd;
}

size_t fuse_chan_bufsize(struct fuse_chan *ch)
{
  return ch->bufsize;
}

struct fuse_session *fuse_chan_session(struct fuse_chan *ch)
{
  return ch->se;
}

int
fuse_chan_recv(struct fuse_chan *ch,
               char             *buf,
               size_t            size)
{
  int err;
  ssize_t res;
  struct fuse_session *se = fuse_chan_session(ch);
  assert(se != NULL);

 restart:
  res = read(fuse_chan_fd(ch), buf, size);
  err = errno;

  if(fuse_session_exited(se))
    return 0;

  if(res == -1)
    {
      /* ENOENT means the operation was interrupted, it's safe
         to restart */
      if (err == ENOENT)
        goto restart;

      if(err == ENODEV)
        {
          se->exited = 1;
          return 0;
        }

      /* Errors occurring during normal operation: EINTR (read
         interrupted), EAGAIN (nonblocking I/O), ENODEV (filesystem
         umounted) */
      if(err != EINTR && err != EAGAIN)
        perror("fuse: reading device");
      return -err;
    }

  if((size_t) res < sizeof(struct fuse_in_header))
    {
      fprintf(stderr, "short read on fuse device\n");
      return -EIO;
    }

  return res;
}

int
fuse_chan_send(struct fuse_chan *ch,
               const struct iovec iov[],
               size_t count)
{
  if(!iov)
    return 0;

  int err;
  ssize_t res;

  res = writev(fuse_chan_fd(ch), iov, count);
  err = errno;

  if(res == -1)
    {
      struct fuse_session *se = fuse_chan_session(ch);

      assert(se != NULL);

      /* ENOENT means the operation was interrupted */
      if(!fuse_session_exited(se) && err != ENOENT)
        perror("fuse: writing device");
      return -err;
    }

  return 0;
}

void
fuse_chan_destroy(struct fuse_chan *ch)
{
  int fd;

  fuse_session_remove_chan(ch);

  fd = fuse_chan_fd(ch);
  if(fd != -1)
    close(fd);

  free(ch);
}
