/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#pragma once

#include "fuse.h"
#include "fuse_lowlevel.h"
#include "fuse_msgbuf_t.h"

#include "extern_c.h"

/* Simplified fuse_session - fields inlined from former fuse_chan
 * Since mergerfs only supports one mount, we collapse the hierarchy:
 * fuse_session -> fuse_chan -> fd
 * into a single structure with direct fields.
 */
struct fuse_session
{
  int (*receive_buf)(struct fuse_session *se,
                     fuse_msgbuf_t       *msgbuf);

  void (*process_buf)(struct fuse_session *se,
                      const fuse_msgbuf_t *msgbuf);

  void (*destroy)(void *data);

  struct fuse_ll *f;
  volatile int exited;

  /* Formerly in fuse_chan - inlined for single-mount simplicity */
  int fd;            /* /dev/fuse file descriptor */
  size_t bufsize;    /* Buffer size for I/O operations */
};

struct fuse_notify_req
{
  uint64_t unique;
  void (*reply)(struct fuse_notify_req *,
                fuse_req_t*,
                uint64_t,
                const void *);
  struct fuse_notify_req *next;
  struct fuse_notify_req *prev;
};



EXTERN_C_BEGIN

struct fuse_session *fuse_lowlevel_new_common(struct fuse_args *args,
                                              const struct fuse_lowlevel_ops *op,
                                              size_t op_size, void *userdata);



void fuse_kern_unmount(const char *mountpoint, int fd);
int fuse_kern_mount(const char *mountpoint, struct fuse_args *args);

int fuse_start_thread(pthread_t *thread_id, void *(*func)(void *), void *arg);

EXTERN_C_END
