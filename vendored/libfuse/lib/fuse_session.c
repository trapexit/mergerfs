/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
 */

#include "fuse_i.h"
#include "fuse_kernel.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>

/*
 * Simplified session management for single-mount filesystems.
 * The fuse_chan structure has been removed and its fields inlined
 * into fuse_session (fd, bufsize) since mergerfs only supports
 * one mount point per process.
 */

struct fuse_session*
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
  se->fd          = -1;  /* Not yet mounted */
  //se->bufsize     = 0;

  return se;
}

void
fuse_session_destroy(struct fuse_session *se)
{
  se->destroy(se->f);
  if(se->fd != -1) {
    close(se->fd);
    se->fd = -1;
  }
  free(se);
}

void
fuse_session_reset(struct fuse_session *se)
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

int
fuse_session_clearfd(struct fuse_session *se)
{
  int fd = se->fd;
  se->fd = -1;
  return fd;
}

void
fuse_session_setfd(struct fuse_session *se,
                   int                   fd)
{
  se->fd = fd;
}

void
fuse_session_setbufsize(struct fuse_session *se,
                        size_t               bufsize)
{
  se->bufsize = bufsize;
}
