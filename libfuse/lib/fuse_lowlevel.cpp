/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "mutex.hpp"
#include "lfmp.h"

#include "config.h"
#include "debug.hpp"
#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_opt.h"
#include "fuse_misc.h"
#include "fuse_pollhandle.h"
#include "fuse_msgbuf.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <sys/file.h>

#ifndef F_LINUX_SPECIFIC_BASE
#define F_LINUX_SPECIFIC_BASE       1024
#endif
#ifndef F_SETPIPE_SZ
#define F_SETPIPE_SZ	(F_LINUX_SPECIFIC_BASE + 7)
#endif


#define PARAM(inarg) (((char*)(inarg)) + sizeof(*(inarg)))
#define OFFSET_MAX 0x7fffffffffffffffLL

#define container_of(ptr, type, member) ({                      \
      const decltype( ((type*)0)->member ) *__mptr = (ptr);       \
      (type *)( (char*)__mptr - offsetof(type,member) );})

static size_t pagesize;
static lfmp_t g_FMP_fuse_req;

static
__attribute__((constructor))
void
fuse_ll_constructor(void)
{
  pagesize = sysconf(_SC_PAGESIZE);
  lfmp_init(&g_FMP_fuse_req,sizeof(struct fuse_req),1);
}

static
__attribute__((destructor))
void
fuse_ll_destructor(void)
{
  lfmp_destroy(&g_FMP_fuse_req);
}


static
void
convert_stat(const struct stat *stbuf_,
             struct fuse_attr  *attr_)
{
  attr_->ino       = stbuf_->st_ino;
  attr_->mode      = stbuf_->st_mode;
  attr_->nlink     = stbuf_->st_nlink;
  attr_->uid       = stbuf_->st_uid;
  attr_->gid       = stbuf_->st_gid;
  attr_->rdev      = stbuf_->st_rdev;
  attr_->size      = stbuf_->st_size;
  attr_->blksize   = stbuf_->st_blksize;
  attr_->blocks    = stbuf_->st_blocks;
  attr_->atime     = stbuf_->st_atime;
  attr_->mtime     = stbuf_->st_mtime;
  attr_->ctime     = stbuf_->st_ctime;
  attr_->atimensec = ST_ATIM_NSEC(stbuf_);
  attr_->mtimensec = ST_MTIM_NSEC(stbuf_);
  attr_->ctimensec = ST_CTIM_NSEC(stbuf_);
}

static
size_t
iov_length(const struct iovec *iov,
           size_t              count)
{
  size_t seg;
  size_t ret = 0;

  for(seg = 0; seg < count; seg++)
    ret += iov[seg].iov_len;
  return ret;
}

static
void
destroy_req(fuse_req_t req)
{
  lfmp_free(&g_FMP_fuse_req,req);
}

static
struct fuse_req*
fuse_ll_alloc_req(struct fuse_ll *f)
{
  struct fuse_req *req;

  req = (struct fuse_req*)lfmp_calloc(&g_FMP_fuse_req);
  if(req == NULL)
    {
      fprintf(stderr, "fuse: failed to allocate request\n");
    }
  else
    {
      req->f = f;
    }

  return req;
}


static
int
fuse_send_msg(struct fuse_ll   *f,
              struct fuse_chan *ch,
              struct iovec     *iov,
              int               count)
{
  int rv;
  struct fuse_out_header *out = (fuse_out_header*)iov[0].iov_base;

  out->len = iov_length(iov, count);

  rv = writev(fuse_chan_fd(ch),iov,count);
  if(rv == -1)
    return -errno;

  return 0;
}

#define MAX_ERRNO 4095

int
fuse_send_reply_iov_nofree(fuse_req_t    req,
                           int           error,
                           struct iovec *iov,
                           int           count)
{
  struct fuse_out_header out;

  if(error > 0)
    error = -error;

  if(error <= -MAX_ERRNO)
    {
      fprintf(stderr,"fuse: bad error value: %i\n",error);
      error = -ERANGE;
    }

  out.unique = req->unique;
  out.error  = error;

  iov[0].iov_base = &out;
  iov[0].iov_len  = sizeof(struct fuse_out_header);

  return fuse_send_msg(req->f, req->ch, iov, count);
}

static
int
send_reply_iov(fuse_req_t    req,
               int           error,
               struct iovec *iov,
               int           count)
{
  int res;

  res = fuse_send_reply_iov_nofree(req, error, iov, count);
  destroy_req(req);

  return res;
}

static
int
send_reply(fuse_req_t  req,
           int         error,
           const void *arg,
           size_t      argsize)
{
  struct iovec iov[2];
  int count = 1;
  if(argsize)
    {
      iov[1].iov_base = (void *) arg;
      iov[1].iov_len = argsize;
      count++;
    }

  return send_reply_iov(req, error, iov, count);
}

static
void
convert_statfs(const struct statvfs *stbuf,
               struct fuse_kstatfs  *kstatfs)
{
  kstatfs->bsize   = stbuf->f_bsize;
  kstatfs->frsize  = stbuf->f_frsize;
  kstatfs->blocks  = stbuf->f_blocks;
  kstatfs->bfree   = stbuf->f_bfree;
  kstatfs->bavail  = stbuf->f_bavail;
  kstatfs->files   = stbuf->f_files;
  kstatfs->ffree   = stbuf->f_ffree;
  kstatfs->namelen = stbuf->f_namemax;
}

static
int
send_reply_ok(fuse_req_t  req,
              const void *arg,
              size_t      argsize)
{
  return send_reply(req, 0, arg, argsize);
}

int
fuse_reply_err(fuse_req_t req_,
               int        err_)
{
  return send_reply(req_,err_,NULL,0);
}

void
fuse_reply_none(fuse_req_t req)
{
  destroy_req(req);
}

static
void
fill_entry(struct fuse_entry_out         *arg,
           const struct fuse_entry_param *e)
{
  arg->nodeid           = e->ino;
  arg->generation       = e->generation;
  arg->entry_valid      = e->timeout.entry;
  arg->entry_valid_nsec = 0;
  arg->attr_valid       = e->timeout.attr;
  arg->attr_valid_nsec  = 0;
  convert_stat(&e->attr,&arg->attr);
}

static
void
fill_open(struct fuse_open_out   *arg_,
          const fuse_file_info_t *ffi_)
{
  arg_->fh = ffi_->fh;
  if(ffi_->direct_io)
    arg_->open_flags |= FOPEN_DIRECT_IO;
  if(ffi_->keep_cache)
    arg_->open_flags |= FOPEN_KEEP_CACHE;
  if(ffi_->nonseekable)
    arg_->open_flags |= FOPEN_NONSEEKABLE;
  if(ffi_->cache_readdir)
    arg_->open_flags |= FOPEN_CACHE_DIR;
  if(ffi_->parallel_direct_writes)
    arg_->open_flags |= FOPEN_PARALLEL_DIRECT_WRITES;
  if(ffi_->noflush)
    arg_->open_flags |= FOPEN_NOFLUSH;
  if(ffi_->passthrough && (ffi_->backing_id > 0))
    {
      arg_->open_flags |= FOPEN_PASSTHROUGH;
      arg_->backing_id  = ffi_->backing_id;
    }
}

int
fuse_reply_entry(fuse_req_t                     req,
                 const struct fuse_entry_param *e)
{
  struct fuse_entry_out arg = {0};
  size_t size = req->f->conn.proto_minor < 9 ?
    FUSE_COMPAT_ENTRY_OUT_SIZE : sizeof(arg);

  /* before ABI 7.4 e->ino == 0 was invalid, only ENOENT meant
     negative entry */
  if(!e->ino && req->f->conn.proto_minor < 4)
    return fuse_reply_err(req, ENOENT);

  fill_entry(&arg, e);

#ifdef NDEBUG
  // TODO: Add checks for cases where a node could be marked bad by
  // the kernel.
#endif

  return send_reply_ok(req, &arg, size);
}

struct fuse_create_out
{
  struct fuse_entry_out e;
  struct fuse_open_out o;
};

int
fuse_reply_create(fuse_req_t                     req,
                  const struct fuse_entry_param *e,
                  const fuse_file_info_t        *f)
{
  struct fuse_create_out buf = {0};
  size_t entrysize = req->f->conn.proto_minor < 9 ?
    FUSE_COMPAT_ENTRY_OUT_SIZE : sizeof(struct fuse_entry_out);
  struct fuse_entry_out *earg = (struct fuse_entry_out*)&buf.e;
  struct fuse_open_out  *oarg = (struct fuse_open_out*)(((char*)&buf)+entrysize);

  fill_entry(earg, e);
  fill_open(oarg, f);

  return send_reply_ok(req, &buf, entrysize + sizeof(struct fuse_open_out));
}

int
fuse_reply_attr(fuse_req_t         req,
                const struct stat *attr,
                const uint64_t     timeout)
{
  struct fuse_attr_out arg = {0};
  size_t size = req->f->conn.proto_minor < 9 ?
    FUSE_COMPAT_ATTR_OUT_SIZE : sizeof(arg);

  arg.attr_valid      = timeout;
  arg.attr_valid_nsec = 0;
  convert_stat(attr,&arg.attr);

  return send_reply_ok(req,&arg,size);
}

int
fuse_reply_statx(fuse_req_t         req_,
                 int                flags_,
                 struct fuse_statx *st_,
                 uint64_t           timeout_)
{
  struct fuse_statx_out outarg{};

  outarg.flags           = flags_;
  outarg.attr_valid      = timeout_;
  outarg.attr_valid_nsec = 0;
  outarg.stat            = *(struct fuse_statx*)st_;

  return send_reply_ok(req_,&outarg,sizeof(outarg));
}

int
fuse_reply_readlink(fuse_req_t  req,
                    const char *linkname)
{
  return send_reply_ok(req, linkname, strlen(linkname));
}

int
fuse_reply_open(fuse_req_t              req,
                const fuse_file_info_t *f)
{
  struct fuse_open_out arg = {0};

  fill_open(&arg, f);

  return send_reply_ok(req, &arg, sizeof(arg));
}

int
fuse_reply_write(fuse_req_t req,
                 size_t     count)
{
  struct fuse_write_out arg = {0};

  arg.size = count;

  return send_reply_ok(req, &arg, sizeof(arg));
}

int
fuse_reply_buf(fuse_req_t  req,
               const char *buf,
               size_t      size)
{
  return send_reply_ok(req, buf, size);
}

static
int
fuse_send_data_iov_fallback(struct fuse_ll     *f,
                            struct fuse_chan   *ch,
                            struct iovec       *iov,
                            int                 iov_count,
                            struct fuse_bufvec *buf,
                            size_t              len)
{
  int res;
  struct fuse_bufvec mem_buf = FUSE_BUFVEC_INIT(len);

  /* Optimize common case */
  if(buf->count == 1 && buf->idx == 0 && buf->off == 0 &&
      !(buf->buf[0].flags & FUSE_BUF_IS_FD))
    {
      /* FIXME: also avoid memory copy if there are multiple buffers
         but none of them contain an fd */

      iov[iov_count].iov_base = buf->buf[0].mem;
      iov[iov_count].iov_len = len;
      iov_count++;

      return fuse_send_msg(f, ch, iov, iov_count);
    }

  fuse_msgbuf_t *msgbuf;
  msgbuf = msgbuf_alloc();
  if(msgbuf == NULL)
    return -ENOMEM;

  mem_buf.buf[0].mem = msgbuf->mem;
  res = fuse_buf_copy(&mem_buf, buf, (fuse_buf_copy_flags)0);
  if(res < 0)
    {
      msgbuf_free(msgbuf);
      return -res;
    }
  len = res;

  iov[iov_count].iov_base = msgbuf->mem;
  iov[iov_count].iov_len = len;
  iov_count++;
  res = fuse_send_msg(f, ch, iov, iov_count);
  msgbuf_free(msgbuf);

  return res;
}

struct fuse_ll_pipe
{
  size_t size;
  int can_grow;
  int pipe[2];
};

static
void
fuse_ll_pipe_free(struct fuse_ll_pipe *llp)
{
  close(llp->pipe[0]);
  close(llp->pipe[1]);
  free(llp);
}

static
int
fuse_send_data_iov(struct fuse_ll     *f,
                   struct fuse_chan   *ch,
                   struct iovec       *iov,
                   int                 iov_count,
                   struct fuse_bufvec *buf,
                   unsigned int        flags)
{
  size_t len = fuse_buf_size(buf);
  (void) flags;

  return fuse_send_data_iov_fallback(f, ch, iov, iov_count, buf, len);
}

int
fuse_reply_data(fuse_req_t    req,
                char         *buf_,
                const size_t  bufsize_)
{
  int res;
  struct iovec iov[2];
  struct fuse_out_header out;

  iov[0].iov_base = &out;
  iov[0].iov_len  = sizeof(struct fuse_out_header);
  iov[1].iov_base = buf_;
  iov[1].iov_len  = bufsize_;

  out.unique = req->unique;
  out.error = 0;

  res = fuse_send_msg(req->f,req->ch,iov,2);
  if(res <= 0)
    {
      destroy_req(req);
      return res;
    }
  else
    {
      return fuse_reply_err(req, res);
    }
}

int
fuse_reply_statfs(fuse_req_t            req,
                  const struct statvfs *stbuf)
{
  struct fuse_statfs_out arg = {0};
  size_t size = req->f->conn.proto_minor < 4 ?
    FUSE_COMPAT_STATFS_SIZE : sizeof(arg);

  convert_statfs(stbuf, &arg.st);

  return send_reply_ok(req, &arg, size);
}

int
fuse_reply_xattr(fuse_req_t req,
                 size_t     count)
{
  struct fuse_getxattr_out arg = {0};

  arg.size = count;

  return send_reply_ok(req, &arg, sizeof(arg));
}

int
fuse_reply_lock(fuse_req_t          req,
                const struct flock *lock)
{
  struct fuse_lk_out arg = {0};

  arg.lk.type = lock->l_type;
  if(lock->l_type != F_UNLCK)
    {
      arg.lk.start = lock->l_start;
      if(lock->l_len == 0)
        arg.lk.end = OFFSET_MAX;
      else
        arg.lk.end = lock->l_start + lock->l_len - 1;
    }
  arg.lk.pid = lock->l_pid;

  return send_reply_ok(req, &arg, sizeof(arg));
}

int
fuse_reply_bmap(fuse_req_t req,
                uint64_t   idx)
{
  struct fuse_bmap_out arg = {0};

  arg.block = idx;

  return send_reply_ok(req, &arg, sizeof(arg));
}

static
struct fuse_ioctl_iovec*
fuse_ioctl_iovec_copy(const struct iovec *iov,
                      size_t              count)
{
  struct fuse_ioctl_iovec *fiov;
  size_t i;

  fiov = (fuse_ioctl_iovec*)malloc(sizeof(fiov[0]) * count);
  if(!fiov)
    return NULL;

  for (i = 0; i < count; i++)
    {
      fiov[i].base = (uintptr_t) iov[i].iov_base;
      fiov[i].len = iov[i].iov_len;
    }

  return fiov;
}

int
fuse_reply_ioctl_retry(fuse_req_t          req,
                       const struct iovec *in_iov,
                       size_t              in_count,
                       const struct iovec *out_iov,
                       size_t              out_count)
{
  struct fuse_ioctl_out arg = {0};
  struct fuse_ioctl_iovec *in_fiov = NULL;
  struct fuse_ioctl_iovec *out_fiov = NULL;
  struct iovec iov[4];
  size_t count = 1;
  int res;

  arg.flags |= FUSE_IOCTL_RETRY;
  arg.in_iovs = in_count;
  arg.out_iovs = out_count;
  iov[count].iov_base = &arg;
  iov[count].iov_len = sizeof(arg);
  count++;

  if(req->f->conn.proto_minor < 16)
    {
      if(in_count)
        {
          iov[count].iov_base = (void *)in_iov;
          iov[count].iov_len = sizeof(in_iov[0]) * in_count;
          count++;
        }

      if(out_count)
        {
          iov[count].iov_base = (void *)out_iov;
          iov[count].iov_len = sizeof(out_iov[0]) * out_count;
          count++;
        }
    }
  else
    {
      /* Can't handle non-compat 64bit ioctls on 32bit */
      if((sizeof(void *) == 4) && (req->ioctl_64bit))
        {
          res = fuse_reply_err(req, EINVAL);
          goto out;
        }

      if(in_count)
        {
          in_fiov = fuse_ioctl_iovec_copy(in_iov, in_count);
          if(!in_fiov)
            goto enomem;

          iov[count].iov_base = (void *)in_fiov;
          iov[count].iov_len = sizeof(in_fiov[0]) * in_count;
          count++;
        }
      if(out_count)
        {
          out_fiov = fuse_ioctl_iovec_copy(out_iov, out_count);
          if(!out_fiov)
            goto enomem;

          iov[count].iov_base = (void *)out_fiov;
          iov[count].iov_len = sizeof(out_fiov[0]) * out_count;
          count++;
        }
    }

  res = send_reply_iov(req, 0, iov, count);
 out:
  free(in_fiov);
  free(out_fiov);

  return res;

 enomem:
  res = fuse_reply_err(req, ENOMEM);
  goto out;
}

int
fuse_reply_ioctl(fuse_req_t  req,
                 int         result,
                 const void *buf,
                 uint32_t    size)
{
  int count;
  struct iovec iov[3];
  struct fuse_ioctl_out arg;

  arg.result   = result;
  arg.flags    = 0;
  arg.in_iovs  = 0;
  arg.out_iovs = 0;

  count = 1;
  iov[count].iov_base = &arg;
  iov[count].iov_len  = sizeof(arg);
  count++;

  if(size)
    {
      iov[count].iov_base = (char*)buf;
      iov[count].iov_len  = size;
      count++;
    }

  return send_reply_iov(req, 0, iov, count);
}

int
fuse_reply_ioctl_iov(fuse_req_t          req,
                     int                 result,
                     const struct iovec *iov,
                     int                 count)
{
  struct iovec *padded_iov;
  struct fuse_ioctl_out arg = {0};
  int res;

  padded_iov = (iovec*)malloc((count + 2) * sizeof(struct iovec));
  if(padded_iov == NULL)
    return fuse_reply_err(req, ENOMEM);

  arg.result = result;
  padded_iov[1].iov_base = &arg;
  padded_iov[1].iov_len = sizeof(arg);

  memcpy(&padded_iov[2], iov, count * sizeof(struct iovec));

  res = send_reply_iov(req, 0, padded_iov, count + 2);
  free(padded_iov);

  return res;
}

int
fuse_reply_poll(fuse_req_t req,
                unsigned   revents)
{
  struct fuse_poll_out arg = {0};

  arg.revents = revents;

  return send_reply_ok(req, &arg, sizeof(arg));
}

static
void
do_lookup(fuse_req_t             req,
          struct fuse_in_header *hdr_)
{
  req->f->op.lookup(req,hdr_);
}

static
void
do_forget(fuse_req_t             req,
          struct fuse_in_header *hdr_)
{
  req->f->op.forget(req,hdr_);
}

static
void
do_batch_forget(fuse_req_t             req,
                struct fuse_in_header *hdr_)
{
  req->f->op.forget_multi(req,hdr_);
}

static
void
do_getattr(fuse_req_t             req,
           struct fuse_in_header *hdr_)
{
  req->f->op.getattr(req, hdr_);
}

static
void
do_setattr(fuse_req_t             req_,
           struct fuse_in_header *hdr_)
{
  req_->f->op.setattr(req_,hdr_);
}

static
void
do_access(fuse_req_t             req,
          struct fuse_in_header *hdr_)
{
  req->f->op.access(req,hdr_);
}

static
void
do_readlink(fuse_req_t             req,
            struct fuse_in_header *hdr_)
{
  req->f->op.readlink(req,hdr_);
}

static
void
do_mknod(fuse_req_t             req,
         struct fuse_in_header *hdr_)
{
  req->f->op.mknod(req,hdr_);
}

static
void
do_mkdir(fuse_req_t             req,
         struct fuse_in_header *hdr_)
{
  req->f->op.mkdir(req,hdr_);
}

static
void
do_unlink(fuse_req_t             req,
          struct fuse_in_header *hdr_)
{
  req->f->op.unlink(req,hdr_);
}

static
void
do_rmdir(fuse_req_t             req,
         struct fuse_in_header *hdr_)
{
  req->f->op.rmdir(req,hdr_);
}

static
void
do_symlink(fuse_req_t             req,
           struct fuse_in_header *hdr_)
{
  req->f->op.symlink(req,hdr_);
}

static
void
do_rename(fuse_req_t             req,
          struct fuse_in_header *hdr_)
{
  req->f->op.rename(req,hdr_);
}

static
void
do_link(fuse_req_t             req,
        struct fuse_in_header *hdr_)
{
  req->f->op.link(req,hdr_);
}

static
void
do_create(fuse_req_t             req,
          struct fuse_in_header *hdr_)
{
  req->f->op.create(req,hdr_);
}

static
void
do_open(fuse_req_t             req,
        struct fuse_in_header *hdr_)
{
  req->f->op.open(req,hdr_);
}

static
void
do_read(fuse_req_t             req,
        struct fuse_in_header *hdr_)
{
  req->f->op.read(req,hdr_);
}

static
void
do_write(fuse_req_t             req,
         struct fuse_in_header *hdr_)
{
  req->f->op.write(req,hdr_);
}

static
void
do_flush(fuse_req_t             req,
         struct fuse_in_header *hdr_)
{
  req->f->op.flush(req,hdr_);
}

static
void
do_release(fuse_req_t             req,
           struct fuse_in_header *hdr_)
{
  req->f->op.release(req,hdr_);
}

static
void
do_fsync(fuse_req_t             req,
         struct fuse_in_header *hdr_)
{
  req->f->op.fsync(req,hdr_);
}

static
void
do_opendir(fuse_req_t             req,
           struct fuse_in_header *hdr_)
{
  req->f->op.opendir(req,hdr_);
}

static
void
do_readdir(fuse_req_t             req,
           struct fuse_in_header *hdr_)
{
  req->f->op.readdir(req,hdr_);
}

static
void
do_readdirplus(fuse_req_t             req_,
               struct fuse_in_header *hdr_)
{
  req_->f->op.readdir_plus(req_,hdr_);
}

static
void
do_releasedir(fuse_req_t             req,
              struct fuse_in_header *hdr_)
{
  req->f->op.releasedir(req,hdr_);
}

static
void
do_fsyncdir(fuse_req_t             req,
            struct fuse_in_header *hdr_)
{
  req->f->op.fsyncdir(req,hdr_);
}

static
void
do_statfs(fuse_req_t             req,
          struct fuse_in_header *hdr_)
{
  req->f->op.statfs(req,hdr_);
}

static
void
do_setxattr(fuse_req_t             req,
            struct fuse_in_header *hdr_)
{
  req->f->op.setxattr(req,hdr_);
}

static
void
do_getxattr(fuse_req_t             req,
            struct fuse_in_header *hdr_)
{
  req->f->op.getxattr(req,hdr_);
}

static
void
do_listxattr(fuse_req_t             req,
             struct fuse_in_header *hdr_)
{
  req->f->op.listxattr(req,hdr_);
}

static
void
do_removexattr(fuse_req_t             req,
               struct fuse_in_header *hdr_)
{
  req->f->op.removexattr(req,hdr_);
}

static
void
convert_fuse_file_lock(struct fuse_file_lock *fl,
                       struct flock          *flock)
{
  memset(flock, 0, sizeof(struct flock));
  flock->l_type = fl->type;
  flock->l_whence = SEEK_SET;
  flock->l_start = fl->start;
  if(fl->end == OFFSET_MAX)
    flock->l_len = 0;
  else
    flock->l_len = fl->end - fl->start + 1;
  flock->l_pid = fl->pid;
}

static
void
do_getlk(fuse_req_t             req,
         struct fuse_in_header *hdr_)
{
  req->f->op.getlk(req,hdr_);
}

static
void
do_setlk_common(fuse_req_t  req,
                uint64_t    nodeid,
                const void *inarg,
                int         sleep)
{
  struct flock flock;
  fuse_file_info_t fi = {0};
  struct fuse_lk_in *arg = (struct fuse_lk_in*)inarg;

  fi.fh = arg->fh;
  fi.lock_owner = arg->owner;

  if(arg->lk_flags & FUSE_LK_FLOCK)
    {
      int op = 0;

      switch (arg->lk.type)
        {
        case F_RDLCK:
          op = LOCK_SH;
          break;
        case F_WRLCK:
          op = LOCK_EX;
          break;
        case F_UNLCK:
          op = LOCK_UN;
          break;
        }

      if(!sleep)
        op |= LOCK_NB;

      req->f->op.flock(req,nodeid,&fi,op);
    }
  else
    {
      convert_fuse_file_lock(&arg->lk, &flock);

      req->f->op.setlk(req,nodeid,&fi,&flock,sleep);
    }
}

static
void
do_setlk(fuse_req_t             req,
         struct fuse_in_header *hdr_)
{
  do_setlk_common(req, hdr_->nodeid, &hdr_[1], 0);
}

static
void
do_setlkw(fuse_req_t             req,
          struct fuse_in_header *hdr_)
{
  do_setlk_common(req, hdr_->nodeid, &hdr_[1], 1);
}

static
void
do_interrupt(fuse_req_t  req,
             struct fuse_in_header *hdr_)
{
  destroy_req(req);
}

static
void
do_bmap(fuse_req_t             req,
        struct fuse_in_header *hdr_)
{
  req->f->op.bmap(req,hdr_);
}

static
void
do_ioctl(fuse_req_t  req,
         struct fuse_in_header *hdr_)
{
  req->f->op.ioctl(req, hdr_);
}

void
fuse_pollhandle_destroy(fuse_pollhandle_t *ph)
{
  free(ph);
}

static
void
do_poll(fuse_req_t             req,
        struct fuse_in_header *hdr_)
{
  req->f->op.poll(req,hdr_);
}

static
void
do_fallocate(fuse_req_t             req,
             struct fuse_in_header *hdr_)
{
  req->f->op.fallocate(req,hdr_);
}

static
void
do_init(fuse_req_t             req,
        struct fuse_in_header *hdr_)
{
  struct fuse_init_out outarg = {0};
  struct fuse_init_in *arg = (struct fuse_init_in *) &hdr_[1];
  struct fuse_ll *f = req->f;
  size_t bufsize = fuse_chan_bufsize(req->ch);
  uint64_t inargflags;
  uint64_t outargflags;

  inargflags = 0;
  outargflags = 0;

  fuse_syslog_fuse_init_in(arg);

  f->conn.proto_major = arg->major;
  f->conn.proto_minor = arg->minor;
  f->conn.capable = 0;
  f->conn.want = 0;

  outarg.major     = FUSE_KERNEL_VERSION;
  outarg.minor     = FUSE_KERNEL_MINOR_VERSION;
  outarg.max_pages = FUSE_DEFAULT_MAX_PAGES_PER_REQ;

  if(arg->major < 7)
    {
      fprintf(stderr, "fuse: unsupported protocol version: %u.%u\n",
              arg->major, arg->minor);
      fuse_reply_err(req, EPROTO);
      return;
    }

  if(arg->major > 7)
    {
      /* Wait for a second INIT request with a 7.X version */
      send_reply_ok(req, &outarg, sizeof(outarg));
      return;
    }

  if(arg->minor >= 6)
    {
      inargflags = arg->flags;
      if(inargflags & FUSE_INIT_EXT)
        inargflags |= (((uint64_t)arg->flags2) << 32);

      if(arg->max_readahead < f->conn.max_readahead)
        f->conn.max_readahead = arg->max_readahead;

      if(inargflags & FUSE_ASYNC_READ)
        f->conn.capable |= FUSE_CAP_ASYNC_READ;
      if(inargflags & FUSE_POSIX_LOCKS)
        f->conn.capable |= FUSE_CAP_POSIX_LOCKS;
      if(inargflags & FUSE_ATOMIC_O_TRUNC)
        f->conn.capable |= FUSE_CAP_ATOMIC_O_TRUNC;
      if(inargflags & FUSE_EXPORT_SUPPORT)
        f->conn.capable |= FUSE_CAP_EXPORT_SUPPORT;
      if(inargflags & FUSE_BIG_WRITES)
        f->conn.capable |= FUSE_CAP_BIG_WRITES;
      if(inargflags & FUSE_DONT_MASK)
        f->conn.capable |= FUSE_CAP_DONT_MASK;
      if(inargflags & FUSE_FLOCK_LOCKS)
        f->conn.capable |= FUSE_CAP_FLOCK_LOCKS;
      if(inargflags & FUSE_POSIX_ACL)
        f->conn.capable |= FUSE_CAP_POSIX_ACL;
      if(inargflags & FUSE_CACHE_SYMLINKS)
        f->conn.capable |= FUSE_CAP_CACHE_SYMLINKS;
      if(inargflags & FUSE_ASYNC_DIO)
        f->conn.capable |= FUSE_CAP_ASYNC_DIO;
      if(inargflags & FUSE_PARALLEL_DIROPS)
        f->conn.capable |= FUSE_CAP_PARALLEL_DIROPS;
      if(inargflags & FUSE_MAX_PAGES)
        f->conn.capable |= FUSE_CAP_MAX_PAGES;
      if(inargflags & FUSE_WRITEBACK_CACHE)
        f->conn.capable |= FUSE_CAP_WRITEBACK_CACHE;
      if(inargflags & FUSE_DO_READDIRPLUS)
        f->conn.capable |= FUSE_CAP_READDIR_PLUS;
      if(inargflags & FUSE_READDIRPLUS_AUTO)
        f->conn.capable |= FUSE_CAP_READDIR_PLUS_AUTO;
      if(inargflags & FUSE_SETXATTR_EXT)
        f->conn.capable |= FUSE_CAP_SETXATTR_EXT;
      if(inargflags & FUSE_DIRECT_IO_ALLOW_MMAP)
        f->conn.capable |= FUSE_CAP_DIRECT_IO_ALLOW_MMAP;
      if(inargflags & FUSE_CREATE_SUPP_GROUP)
        f->conn.capable |= FUSE_CAP_CREATE_SUPP_GROUP;
      if(inargflags & FUSE_PASSTHROUGH)
        f->conn.capable |= FUSE_CAP_PASSTHROUGH;
      if(inargflags & FUSE_HANDLE_KILLPRIV)
        f->conn.capable |= FUSE_CAP_HANDLE_KILLPRIV;
      if(inargflags & FUSE_HANDLE_KILLPRIV_V2)
        f->conn.capable |= FUSE_CAP_HANDLE_KILLPRIV_V2;
    }
  else
    {
      f->conn.want &= ~FUSE_CAP_ASYNC_READ;
      f->conn.max_readahead = 0;
    }

  if(req->f->conn.proto_minor >= 18)
    f->conn.capable |= FUSE_CAP_IOCTL_DIR;

  if(f->op.getlk && f->op.setlk && !f->no_remote_posix_lock)
    f->conn.want |= FUSE_CAP_POSIX_LOCKS;
  if(f->op.flock && !f->no_remote_flock)
    f->conn.want |= FUSE_CAP_FLOCK_LOCKS;

  if(bufsize < FUSE_MIN_READ_BUFFER)
    {
      fprintf(stderr, "fuse: warning: buffer size too small: %zu\n",
              bufsize);
      bufsize = FUSE_MIN_READ_BUFFER;
    }

  bufsize -= pagesize;
  if(bufsize < f->conn.max_write)
    f->conn.max_write = bufsize;

  f->got_init = 1;
  if(f->op.init)
    f->op.init(f->userdata, &f->conn);

  outargflags = outarg.flags;
  if((inargflags & FUSE_MAX_PAGES) && (f->conn.want & FUSE_CAP_MAX_PAGES))
    {
      outargflags     |= FUSE_MAX_PAGES;
      outarg.max_pages  = f->conn.max_pages;

      msgbuf_set_bufsize(outarg.max_pages + 1);
    }

  if(f->conn.want & FUSE_CAP_ASYNC_READ)
    outargflags |= FUSE_ASYNC_READ;
  if(f->conn.want & FUSE_CAP_POSIX_LOCKS)
    outargflags |= FUSE_POSIX_LOCKS;
  if(f->conn.want & FUSE_CAP_ATOMIC_O_TRUNC)
    outargflags |= FUSE_ATOMIC_O_TRUNC;
  if(f->conn.want & FUSE_CAP_EXPORT_SUPPORT)
    outargflags |= FUSE_EXPORT_SUPPORT;
  if(f->conn.want & FUSE_CAP_BIG_WRITES)
    outargflags |= FUSE_BIG_WRITES;
  if(f->conn.want & FUSE_CAP_DONT_MASK)
    outargflags |= FUSE_DONT_MASK;
  if(f->conn.want & FUSE_CAP_FLOCK_LOCKS)
    outargflags |= FUSE_FLOCK_LOCKS;
  if(f->conn.want & FUSE_CAP_POSIX_ACL)
    outargflags |= FUSE_POSIX_ACL;
  if(f->conn.want & FUSE_CAP_CACHE_SYMLINKS)
    outargflags |= FUSE_CACHE_SYMLINKS;
  if(f->conn.want & FUSE_CAP_ASYNC_DIO)
    outargflags |= FUSE_ASYNC_DIO;
  if(f->conn.want & FUSE_CAP_PARALLEL_DIROPS)
    outargflags |= FUSE_PARALLEL_DIROPS;
  if(f->conn.want & FUSE_CAP_WRITEBACK_CACHE)
    outargflags |= FUSE_WRITEBACK_CACHE;
  if(f->conn.want & FUSE_CAP_READDIR_PLUS)
    outargflags |= FUSE_DO_READDIRPLUS;
  if(f->conn.want & FUSE_CAP_READDIR_PLUS_AUTO)
    outargflags |= FUSE_READDIRPLUS_AUTO;
  if(f->conn.want & FUSE_CAP_SETXATTR_EXT)
    outargflags |= FUSE_SETXATTR_EXT;
  if(f->conn.want & FUSE_CAP_CREATE_SUPP_GROUP)
    outargflags |= FUSE_CREATE_SUPP_GROUP;
  if(f->conn.want & FUSE_CAP_DIRECT_IO_ALLOW_MMAP)
    outargflags |= FUSE_DIRECT_IO_ALLOW_MMAP;
  if(f->conn.want & FUSE_CAP_HANDLE_KILLPRIV)
    outargflags |= FUSE_HANDLE_KILLPRIV;
  if(f->conn.want & FUSE_CAP_HANDLE_KILLPRIV_V2)
    outargflags |= FUSE_HANDLE_KILLPRIV_V2;
  if(f->conn.want & FUSE_CAP_PASSTHROUGH)
    {
      outargflags |= FUSE_PASSTHROUGH;
      outarg.max_stack_depth = 2;
    }

  if(inargflags & FUSE_INIT_EXT)
    {
      outargflags |= FUSE_INIT_EXT;
      outarg.flags2 = (outargflags >> 32);
    }

  outarg.flags = outargflags;

  outarg.max_readahead = f->conn.max_readahead;
  outarg.max_write = f->conn.max_write;
  if(f->conn.proto_minor >= 13)
    {
      if(f->conn.max_background >= (1 << 16))
        f->conn.max_background = (1 << 16) - 1;
      if(f->conn.congestion_threshold > f->conn.max_background)
        f->conn.congestion_threshold = f->conn.max_background;
      if(!f->conn.congestion_threshold)
        {
          f->conn.congestion_threshold = f->conn.max_background * 3 / 4;
        }

      outarg.max_background = f->conn.max_background;
      outarg.congestion_threshold = f->conn.congestion_threshold;
    }

  if(f->conn.proto_minor >= 23)
    outarg.time_gran = 1;

  size_t outargsize;
  if(arg->minor < 5)
    outargsize = FUSE_COMPAT_INIT_OUT_SIZE;
  else if(arg->minor < 23)
    outargsize = FUSE_COMPAT_22_INIT_OUT_SIZE;
  else
    outargsize = sizeof(outarg);

  fuse_syslog_fuse_init_out(&outarg);

  send_reply_ok(req, &outarg, outargsize);
}

static
void
do_destroy(fuse_req_t             req,
           struct fuse_in_header *hdr_)
{
  struct fuse_ll *f = req->f;

  f->got_destroy = 1;
  f->op.destroy(f->userdata);

  send_reply_ok(req,NULL,0);
}

static
void
list_del_nreq(struct fuse_notify_req *nreq)
{
  struct fuse_notify_req *prev = nreq->prev;
  struct fuse_notify_req *next = nreq->next;
  prev->next = next;
  next->prev = prev;
}

static
void
list_add_nreq(struct fuse_notify_req *nreq,
              struct fuse_notify_req *next)
{
  struct fuse_notify_req *prev = next->prev;
  nreq->next = next;
  nreq->prev = prev;
  prev->next = nreq;
  next->prev = nreq;
}

static
void
list_init_nreq(struct fuse_notify_req *nreq)
{
  nreq->next = nreq;
  nreq->prev = nreq;
}

static
void
do_notify_reply(fuse_req_t             req,
                struct fuse_in_header *hdr_)
{
  struct fuse_ll *f = req->f;
  struct fuse_notify_req *nreq;
  struct fuse_notify_req *head;

  mutex_lock(&f->lock);
  head = &f->notify_list;
  for(nreq = head->next; nreq != head; nreq = nreq->next)
    {
      if(nreq->unique == req->unique)
        {
          list_del_nreq(nreq);
          break;
        }
    }
  mutex_unlock(&f->lock);

  if(nreq != head)
    nreq->reply(nreq, req, hdr_->nodeid, &hdr_[1]);
}

static
void
do_copy_file_range(fuse_req_t             req_,
                   struct fuse_in_header *hdr_)
{
  req_->f->op.copy_file_range(req_,hdr_);
}

static
void
do_setupmapping(fuse_req_t                req_,
                   struct fuse_in_header *hdr_)
{
  req_->f->op.setupmapping(req_,hdr_);
}

static
void
do_removemapping(fuse_req_t             req_,
                 struct fuse_in_header *hdr_)
{
  req_->f->op.removemapping(req_,hdr_);
}

static
void
do_syncfs(fuse_req_t             req_,
          struct fuse_in_header *hdr_)
{
  req_->f->op.syncfs(req_,hdr_);
}

static
void
do_tmpfile(fuse_req_t             req_,
           struct fuse_in_header *hdr_)
{
  req_->f->op.tmpfile(req_,hdr_);
}

static
void
do_statx(fuse_req_t             req_,
         struct fuse_in_header *hdr_)
{
  req_->f->op.statx(req_,hdr_);
}

static
void
do_rename2(fuse_req_t             req_,
           struct fuse_in_header *hdr_)
{
  req_->f->op.rename2(req_,hdr_);
}

static
void
do_lseek(fuse_req_t             req_,
         struct fuse_in_header *hdr_)
{
  req_->f->op.lseek(req_,hdr_);
}

static
int
send_notify_iov(struct fuse_ll   *f,
                struct fuse_chan *ch,
                int               notify_code,
                struct iovec     *iov,
                int               count)
{
  struct fuse_out_header out;

  if(!f->got_init)
    return -ENOTCONN;

  out.unique = 0;
  out.error = notify_code;
  iov[0].iov_base = &out;
  iov[0].iov_len = sizeof(struct fuse_out_header);

  return fuse_send_msg(f, ch, iov, count);
}

int
fuse_lowlevel_notify_poll(fuse_pollhandle_t *ph)
{
  if(ph != NULL)
    {
      struct fuse_notify_poll_wakeup_out outarg;
      struct iovec iov[2];

      outarg.kh = ph->kh;

      iov[1].iov_base = &outarg;
      iov[1].iov_len = sizeof(outarg);

      return send_notify_iov(ph->f, ph->ch, FUSE_NOTIFY_POLL, iov, 2);
    }
  else
    {
      return 0;
    }
}

int
fuse_lowlevel_notify_inval_inode(struct fuse_chan *ch,
                                 uint64_t          ino,
                                 off_t             off,
                                 off_t             len)
{
  struct fuse_notify_inval_inode_out outarg;
  struct fuse_ll *f;
  struct iovec iov[2];

  if(!ch)
    return -EINVAL;

  f = (struct fuse_ll*)fuse_session_data(fuse_chan_session(ch));
  if(!f)
    return -ENODEV;

  outarg.ino = ino;
  outarg.off = off;
  outarg.len = len;

  iov[1].iov_base = &outarg;
  iov[1].iov_len  = sizeof(outarg);

  return send_notify_iov(f, ch, FUSE_NOTIFY_INVAL_INODE, iov, 2);
}

int
fuse_lowlevel_notify_inval_entry(struct fuse_chan *ch,
                                 uint64_t          parent,
                                 const char       *name,
                                 size_t            namelen)
{
  struct fuse_notify_inval_entry_out outarg;
  struct fuse_ll *f;
  struct iovec iov[3];

  if(!ch)
    return -EINVAL;

  f = (struct fuse_ll*)fuse_session_data(fuse_chan_session(ch));
  if(!f)
    return -ENODEV;

  outarg.parent  = parent;
  outarg.namelen = namelen;
  // TODO: Add ability to set `flags`
  outarg.flags   = 0;

  iov[1].iov_base = &outarg;
  iov[1].iov_len = sizeof(outarg);
  iov[2].iov_base = (void *)name;
  iov[2].iov_len = namelen + 1;

  return send_notify_iov(f, ch, FUSE_NOTIFY_INVAL_ENTRY, iov, 3);
}

int
fuse_lowlevel_notify_delete(struct fuse_chan *ch,
                            uint64_t          parent,
                            uint64_t          child,
                            const char       *name,
                            size_t            namelen)
{
  struct fuse_notify_delete_out outarg;
  struct fuse_ll *f;
  struct iovec iov[3];

  if(!ch)
    return -EINVAL;

  f = (struct fuse_ll*)fuse_session_data(fuse_chan_session(ch));
  if(!f)
    return -ENODEV;

  if(f->conn.proto_minor < 18)
    return -ENOSYS;

  outarg.parent = parent;
  outarg.child = child;
  outarg.namelen = namelen;
  outarg.padding = 0;

  iov[1].iov_base = &outarg;
  iov[1].iov_len = sizeof(outarg);
  iov[2].iov_base = (void *)name;
  iov[2].iov_len = namelen + 1;

  return send_notify_iov(f, ch, FUSE_NOTIFY_DELETE, iov, 3);
}

int
fuse_lowlevel_notify_store(struct fuse_chan         *ch,
                           uint64_t                  ino,
                           off_t                     offset,
                           struct fuse_bufvec       *bufv,
                           enum fuse_buf_copy_flags  flags)
{
  struct fuse_out_header out;
  struct fuse_notify_store_out outarg;
  struct fuse_ll *f;
  struct iovec iov[3];
  size_t size = fuse_buf_size(bufv);
  int res;

  if(!ch)
    return -EINVAL;

  f = (struct fuse_ll*)fuse_session_data(fuse_chan_session(ch));
  if(!f)
    return -ENODEV;

  if(f->conn.proto_minor < 15)
    return -ENOSYS;

  out.unique = 0;
  out.error = FUSE_NOTIFY_STORE;

  outarg.nodeid = ino;
  outarg.offset = offset;
  outarg.size = size;
  outarg.padding = 0;

  iov[0].iov_base = &out;
  iov[0].iov_len = sizeof(out);
  iov[1].iov_base = &outarg;
  iov[1].iov_len = sizeof(outarg);

  res = fuse_send_data_iov(f, ch, iov, 2, bufv, flags);
  if(res > 0)
    res = -res;

  return res;
}

struct fuse_retrieve_req
{
  struct fuse_notify_req nreq;
  void *cookie;
};

static
void
fuse_ll_retrieve_reply(struct fuse_notify_req *nreq,
                       fuse_req_t              req,
                       uint64_t                ino,
                       const void             *inarg)
{
  struct fuse_retrieve_req *rreq =
    container_of(nreq, struct fuse_retrieve_req, nreq);

  fuse_reply_none(req);

  free(rreq);
}

int
fuse_lowlevel_notify_retrieve(struct fuse_chan *ch,
                              uint64_t          ino,
                              size_t            size,
                              off_t             offset,
                              void             *cookie)
{
  struct fuse_notify_retrieve_out outarg;
  struct fuse_ll *f;
  struct iovec iov[2];
  struct fuse_retrieve_req *rreq;
  int err;

  if(!ch)
    return -EINVAL;

  f = (struct fuse_ll*)fuse_session_data(fuse_chan_session(ch));
  if(!f)
    return -ENODEV;

  if(f->conn.proto_minor < 15)
    return -ENOSYS;

  rreq = (fuse_retrieve_req*)malloc(sizeof(*rreq));
  if(rreq == NULL)
    return -ENOMEM;

  mutex_lock(&f->lock);
  rreq->cookie = cookie;
  rreq->nreq.unique = f->notify_ctr++;
  rreq->nreq.reply = fuse_ll_retrieve_reply;
  list_add_nreq(&rreq->nreq, &f->notify_list);
  mutex_unlock(&f->lock);

  outarg.notify_unique = rreq->nreq.unique;
  outarg.nodeid = ino;
  outarg.offset = offset;
  outarg.size = size;

  iov[1].iov_base = &outarg;
  iov[1].iov_len = sizeof(outarg);

  err = send_notify_iov(f, ch, FUSE_NOTIFY_RETRIEVE, iov, 2);
  if(err)
    {
      mutex_lock(&f->lock);
      list_del_nreq(&rreq->nreq);
      mutex_unlock(&f->lock);
      free(rreq);
    }

  return err;
}

void *
fuse_req_userdata(fuse_req_t req)
{
  return req->f->userdata;
}

const
struct fuse_ctx *
fuse_req_ctx(fuse_req_t req)
{
  return &req->ctx;
}

#define FUSE_OPCODE_LEN (FUSE_STATX + 1)

typedef void (*fuse_ll_func)(fuse_req_t, struct fuse_in_header *);
const
fuse_ll_func
fuse_ll_funcs[FUSE_OPCODE_LEN] =
  {
    NULL,
    do_lookup,
    do_forget,
    do_getattr,
    do_setattr,
    do_readlink,
    do_symlink,
    NULL,
    do_mknod,
    do_mkdir,
    do_unlink,
    do_rmdir,
    do_rename,
    do_link,
    do_open,
    do_read,
    do_write,
    do_statfs,
    do_release,
    NULL,
    do_fsync,
    do_setxattr,
    do_getxattr,
    do_listxattr,
    do_removexattr,
    do_flush,
    do_init,
    do_opendir,
    do_readdir,
    do_releasedir,
    do_fsyncdir,
    do_getlk,
    do_setlk,
    do_setlkw,
    do_access,
    do_create,
    do_interrupt,
    do_bmap,
    do_destroy,
    do_ioctl,
    do_poll,
    do_notify_reply,
    do_batch_forget,
    do_fallocate,
    do_readdirplus,
    do_rename2,
    do_lseek,
    do_copy_file_range,
    do_setupmapping,
    do_removemapping,
    do_syncfs,
    do_tmpfile,
    do_statx
  };

#define FUSE_MAXOPS (sizeof(fuse_ll_funcs) / sizeof(fuse_ll_funcs[0]))

enum {
  KEY_HELP,
  KEY_VERSION,
};

static const struct fuse_opt fuse_ll_opts[] =
  {
    { "debug", offsetof(struct fuse_ll, debug), 1 },
    { "-d", offsetof(struct fuse_ll, debug), 1 },
    { "max_readahead=%u", offsetof(struct fuse_ll, conn.max_readahead), 0 },
    { "max_background=%u", offsetof(struct fuse_ll, conn.max_background), 0 },
    { "congestion_threshold=%u",
      offsetof(struct fuse_ll, conn.congestion_threshold), 0 },
    { "no_remote_lock", offsetof(struct fuse_ll, no_remote_posix_lock), 1},
    { "no_remote_lock", offsetof(struct fuse_ll, no_remote_flock), 1},
    { "no_remote_flock", offsetof(struct fuse_ll, no_remote_flock), 1},
    { "no_remote_posix_lock", offsetof(struct fuse_ll, no_remote_posix_lock), 1},
    FUSE_OPT_KEY("max_read=", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("-h", KEY_HELP),
    FUSE_OPT_KEY("--help", KEY_HELP),
    FUSE_OPT_KEY("-V", KEY_VERSION),
    FUSE_OPT_KEY("--version", KEY_VERSION),
    FUSE_OPT_END
  };

static
void
fuse_ll_version(void)
{
  fprintf(stderr, "using FUSE kernel interface version %i.%i\n",
          FUSE_KERNEL_VERSION, FUSE_KERNEL_MINOR_VERSION);
}

static
void
fuse_ll_help(void)
{
  fprintf(stderr,
          "    -o max_readahead=N     set maximum readahead\n"
          "    -o max_background=N    set number of maximum background requests\n"
          "    -o congestion_threshold=N  set kernel's congestion threshold\n"
          "    -o no_remote_lock      disable remote file locking\n"
          "    -o no_remote_flock     disable remote file locking (BSD)\n"
          "    -o no_remote_posix_lock disable remove file locking (POSIX)\n"
          );
}

static
int
fuse_ll_opt_proc(void             *data,
                 const char       *arg,
                 int               key,
                 struct fuse_args *outargs)
{
  (void) data; (void) outargs;

  switch (key)
    {
    case KEY_HELP:
      fuse_ll_help();
      break;

    case KEY_VERSION:
      fuse_ll_version();
      break;

    default:
      fprintf(stderr, "fuse: unknown option `%s'\n", arg);
    }

  return -1;
}

int
fuse_lowlevel_is_lib_option(const char *opt)
{
  return fuse_opt_match(fuse_ll_opts, opt);
}

static
void
fuse_ll_destroy(void *data)
{
  struct fuse_ll *f = (struct fuse_ll *)data;
  struct fuse_ll_pipe *llp;

  if(f->got_init && !f->got_destroy)
    {
      if(f->op.destroy)
        f->op.destroy(f->userdata);
    }

  llp = (fuse_ll_pipe*)pthread_getspecific(f->pipe_key);
  if(llp != NULL)
    fuse_ll_pipe_free(llp);
  pthread_key_delete(f->pipe_key);
  mutex_destroy(&f->lock);
  free(f);

  lfmp_clear(&g_FMP_fuse_req);
}

static
void
fuse_ll_pipe_destructor(void *data)
{
  struct fuse_ll_pipe *llp = (fuse_ll_pipe*)data;
  fuse_ll_pipe_free(llp);
}

static
void
fuse_send_errno(struct fuse_ll   *f_,
                struct fuse_chan *ch_,
                const int         errno_,
                const uint64_t    unique_id_)
{
  struct fuse_out_header out = {0};
  struct iovec           iov = {0};

  out.unique   = unique_id_;
  out.error    = -errno_;
  iov.iov_base = &out;
  iov.iov_len  = sizeof(struct fuse_out_header);

  fuse_send_msg(f_,ch_,&iov,1);
}

static
void
fuse_send_enomem(struct fuse_ll   *f_,
                 struct fuse_chan *ch_,
                 const uint64_t    unique_id_)
{
  fuse_send_errno(f_,ch_,ENOMEM,unique_id_);
}

static
int
fuse_ll_buf_receive_read(struct fuse_session *se_,
                         fuse_msgbuf_t       *msgbuf_)
{
  int rv;

  rv = read(fuse_chan_fd(se_->ch),msgbuf_->mem,msgbuf_->size);
  if(rv == -1)
    return -errno;

  if(rv < (int)sizeof(struct fuse_in_header))
    {
      fprintf(stderr, "short read from fuse device\n");
      return -EIO;
    }

  return rv;
}

static
void
fuse_ll_buf_process_read(struct fuse_session *se_,
                         const fuse_msgbuf_t *msgbuf_)
{
  int err;
  struct fuse_req *req;
  struct fuse_in_header *in;

  in = (struct fuse_in_header*)msgbuf_->mem;

  req = fuse_ll_alloc_req(se_->f);
  if(req == NULL)
    return fuse_send_enomem(se_->f,se_->ch,in->unique);

  req->unique     = in->unique;
  req->ctx.opcode = in->opcode;
  req->ctx.unique = in->unique;
  req->ctx.nodeid = in->nodeid;
  req->ctx.uid    = in->uid;
  req->ctx.gid    = in->gid;
  req->ctx.pid    = in->pid;
  req->ch         = se_->ch;

  err = ENOSYS;
  if(in->opcode >= FUSE_MAXOPS)
    goto reply_err;
  if(fuse_ll_funcs[in->opcode] == NULL)
    goto reply_err;

  fuse_ll_funcs[in->opcode](req, in);

  return;

 reply_err:
  fuse_reply_err(req, err);
  return;
}

static
void
fuse_ll_buf_process_read_init(struct fuse_session *se_,
                              const fuse_msgbuf_t *msgbuf_)
{
  int err;
  struct fuse_req *req;
  struct fuse_in_header *in;

  in = (struct fuse_in_header*)msgbuf_->mem;

  req = fuse_ll_alloc_req(se_->f);
  if(req == NULL)
    return fuse_send_enomem(se_->f,se_->ch,in->unique);

  req->unique     = in->unique;
  req->ctx.opcode = in->opcode;
  req->ctx.unique = in->unique;
  req->ctx.nodeid = in->nodeid;
  req->ctx.uid    = in->uid;
  req->ctx.gid    = in->gid;
  req->ctx.pid    = in->pid;
  req->ch         = se_->ch;

  err = EIO;
  if(in->opcode != FUSE_INIT)
    goto reply_err;
  if(fuse_ll_funcs[FUSE_INIT] == NULL)
    goto reply_err;

  se_->process_buf = fuse_ll_buf_process_read;

  fuse_ll_funcs[in->opcode](req,in);

  return;

 reply_err:
  fuse_reply_err(req, err);
  return;
}

/*
 * always call fuse_lowlevel_new_common() internally, to work around a
 * misfeature in the FreeBSD runtime linker, which links the old
 * version of a symbol to internal references.
 */
struct fuse_session *
fuse_lowlevel_new_common(struct fuse_args               *args,
                         const struct fuse_lowlevel_ops *op,
                         size_t                          op_size,
                         void                           *userdata)
{
  int err;
  struct fuse_ll *f;
  struct fuse_session *se;

  if(sizeof(struct fuse_lowlevel_ops) < op_size)
    {
      fprintf(stderr, "fuse: warning: library too old, some operations may not work\n");
      op_size = sizeof(struct fuse_lowlevel_ops);
    }

  f = (struct fuse_ll *) calloc(1, sizeof(struct fuse_ll));
  if(f == NULL)
    {
      fprintf(stderr, "fuse: failed to allocate fuse object\n");
      goto out;
    }

  f->conn.max_write = UINT_MAX;
  f->conn.max_readahead = UINT_MAX;
  list_init_nreq(&f->notify_list);
  f->notify_ctr = 1;
  mutex_init(&f->lock);

  err = pthread_key_create(&f->pipe_key, fuse_ll_pipe_destructor);
  if(err)
    {
      fprintf(stderr, "fuse: failed to create thread specific key: %s\n",
              strerror(err));
      goto out_free;
    }

  if(fuse_opt_parse(args, f, fuse_ll_opts, fuse_ll_opt_proc) == -1)
    goto out_key_destroy;

  memcpy(&f->op, op, op_size);
  f->owner = getuid();
  f->userdata = userdata;

  se = fuse_session_new(f,
                        (void*)fuse_ll_buf_receive_read,
                        (void*)fuse_ll_buf_process_read_init,
                        (void*)fuse_ll_destroy);

  if(!se)
    goto out_key_destroy;

  return se;

 out_key_destroy:
  pthread_key_delete(f->pipe_key);
 out_free:
  mutex_destroy(&f->lock);
  free(f);
 out:
  return NULL;
}

struct fuse_session*
fuse_lowlevel_new(struct fuse_args               *args,
                  const struct fuse_lowlevel_ops *op,
                  size_t                          op_size,
                  void                           *userdata)
{
  return fuse_lowlevel_new_common(args, op, op_size, userdata);
}
