/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#include "fuse_i.hpp"
#include "fuse_kernel.h"
#include "syslog.hpp"

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <new>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/uio.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

/*
 * Simplified session management for single-mount filesystems.
 * The fuse_chan structure has been removed and its fields inlined
 * into fuse_session (fd, bufsize) since mergerfs only supports
 * one mount point per process.
 */

static
int
_clone_fuse_fd(const int fd_)
{
  int rv;
  int clone_fd;
  uint32_t source_fd;

  clone_fd = ::open("/dev/fuse",O_RDWR|O_CLOEXEC);
  if(clone_fd == -1)
    return -errno;

  source_fd = fd_;
  rv = ::ioctl(clone_fd,FUSE_DEV_IOC_CLONE,&source_fd);
  if(rv == -1)
    {
      int err = errno;
      ::close(clone_fd);
      return -err;
    }

  return clone_fd;
}

static
void
_close_clone_fds(struct fuse_session *se_)
{
  for(auto fd : se_->clone_fds)
    {
      if(fd != -1)
        ::close(fd);
    }
  se_->clone_fds.clear();
}

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
  se->receive_buf = reinterpret_cast<int(*)(struct fuse_session*, int, fuse_msgbuf_t*)>(receive_buf);
  se->process_buf = reinterpret_cast<void(*)(struct fuse_session*, int, const fuse_msgbuf_t*)>(process_buf);
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

  ::_close_clone_fds(se);

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
  ::_close_clone_fds(se);

  int fd = se->fd;
  se->fd = -1;
  return fd;
}

void
fuse_session_setfd(struct fuse_session *se,
                   int                   fd)
{
  ::_close_clone_fds(se);

  se->fd = fd;
}

bool
fuse_session_setup_read_fds(struct fuse_session *se_,
                            const int            read_thread_count_)
{
  ::_close_clone_fds(se_);

  if(read_thread_count_ <= 1)
    return false;

  se_->clone_fds.reserve(read_thread_count_ - 1);

  for(auto i = 1; i < read_thread_count_; i++)
    {
      int fd;

      fd = ::_clone_fuse_fd(se_->fd);
      if(fd < 0)
        {
          int err = -fd;

          ::_close_clone_fds(se_);
          SysLog::warning("failed to clone /dev/fuse fd for read threads; "
                          "using shared fd: {} ({})",
                          strerror(err),
                          err);
          return false;
        }

      se_->clone_fds.push_back(fd);
    }

  return true;
}

int
fuse_session_read_fd(struct fuse_session *se_,
                     const int            index_)
{
  if((index_ > 0) && (index_ <= (int)se_->clone_fds.size()))
    return se_->clone_fds[index_ - 1];

  return se_->fd;
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
