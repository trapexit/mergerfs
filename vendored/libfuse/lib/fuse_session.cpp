/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#include "fuse_i.hpp"
#include "fuse_kernel.h"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
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
  auto *se = new(std::nothrow) fuse_session();

  if(se == nullptr)
    {
      fprintf(stderr, "fuse: failed to allocate session\n");
      return nullptr;
    }

  se->f           = static_cast<struct fuse_ll*>(data);
  se->receive_buf = reinterpret_cast<int(*)(struct fuse_session*, fuse_msgbuf_t*)>(receive_buf);
  se->process_buf = reinterpret_cast<void(*)(struct fuse_session*, const fuse_msgbuf_t*)>(process_buf);
  se->destroy     = reinterpret_cast<void(*)(void*)>(destroy);
  se->fd          = -1;

  return se;
}

void
fuse_session_destroy(struct fuse_session *se)
{
  if(se == nullptr)
    return;

  if(se->destroy)
    se->destroy(se->f);

  if(se->fd != -1)
    {
      close(se->fd);
      se->fd = -1;
    }

  delete se;
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

int
fuse_session_fd(struct fuse_session *se)
{
  return se->fd;
}

size_t
fuse_session_bufsize(struct fuse_session *se)
{
  return se->bufsize;
}
