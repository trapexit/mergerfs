/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#pragma once

#include "fuse.h"
#include "fuse_lowlevel.h"
#include "fuse_msgbuf.h"

#include "extern_c.h"

struct fuse_chan;
struct fuse_ll;

struct fuse_session
{
  int (*receive_buf)(struct fuse_session *se,
                     fuse_msgbuf_t       *msgbuf);

  void (*process_buf)(struct fuse_session *se,
                      const fuse_msgbuf_t *msgbuf);

  void (*destroy)(void *data);

  struct fuse_ll *f;
  volatile int exited;
  struct fuse_chan *ch;
};

struct fuse_req
{
  struct fuse_ll *f;
  uint64_t unique;
  struct fuse_ctx ctx;
  struct fuse_chan *ch;
  unsigned int ioctl_64bit : 1;
};

struct fuse_notify_req
{
  uint64_t unique;
  void (*reply)(struct fuse_notify_req *,
                fuse_req_t,
                uint64_t,
                const void *);
  struct fuse_notify_req *next;
  struct fuse_notify_req *prev;
};

struct fuse_ll
{
  int debug;
  int no_remote_posix_lock;
  int no_remote_flock;
  int big_writes;
  struct fuse_lowlevel_ops op;
  void *userdata;
  uid_t owner;
  struct fuse_conn_info conn;
  pthread_mutex_t lock;
  int got_init;
  int got_destroy;
  pthread_key_t pipe_key;
  int broken_splice_nonblock;
  uint64_t notify_ctr;
  struct fuse_notify_req notify_list;
};

struct fuse_cmd
{
  char *buf;
  size_t buflen;
  struct fuse_chan *ch;
};

EXTERN_C_BEGIN

struct fuse *fuse_new_common(struct fuse_chan *ch, struct fuse_args *args,
			     const struct fuse_operations *op,
			     size_t op_size);

struct fuse_chan *fuse_kern_chan_new(int fd);

struct fuse_session *fuse_lowlevel_new_common(struct fuse_args *args,
                                              const struct fuse_lowlevel_ops *op,
                                              size_t op_size, void *userdata);

int fuse_chan_clearfd(struct fuse_chan *ch);

void fuse_kern_unmount(const char *mountpoint, int fd);
int fuse_kern_mount(const char *mountpoint, struct fuse_args *args);

int fuse_send_reply_iov_nofree(fuse_req_t req, int error, struct iovec *iov,
			       int count);
void fuse_free_req(fuse_req_t req);


struct fuse *fuse_setup_common(int argc, char *argv[],
			       const struct fuse_operations *op,
			       size_t op_size,
			       char **mountpoint,
			       int *fd);

int fuse_start_thread(pthread_t *thread_id, void *(*func)(void *), void *arg);

EXTERN_C_END
