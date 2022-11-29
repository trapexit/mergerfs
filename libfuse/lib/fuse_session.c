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

struct fuse_session *
fuse_session_new(void *data,
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
  se->f           = data;
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
  se->destroy(se->f);
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

void*
fuse_session_data(struct fuse_session *se)
{
  return se->f;
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
