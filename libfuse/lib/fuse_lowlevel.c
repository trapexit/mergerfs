/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#define _GNU_SOURCE

#include "lfmp.h"

#include "config.h"
#include "debug.h"
#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_opt.h"
#include "fuse_misc.h"

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


#define PARAM(inarg) (((char *)(inarg)) + sizeof(*(inarg)))
#define OFFSET_MAX 0x7fffffffffffffffLL

#define container_of(ptr, type, member) ({                      \
      const typeof( ((type *)0)->member ) *__mptr = (ptr);      \
      (type *)( (char *)__mptr - offsetof(type,member) );})

struct fuse_pollhandle_t
{
  uint64_t kh;
  struct fuse_chan *ch;
  struct fuse_ll *f;
};

static size_t pagesize;
static lfmp_t g_FMP_fuse_req;

static
__attribute__((constructor))
void
fuse_ll_constructor(void)
{
  pagesize = getpagesize();
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
void
convert_attr(const struct fuse_setattr_in *attr_,
             struct stat                  *stbuf_)
{
  stbuf_->st_mode  = attr_->mode;
  stbuf_->st_uid   = attr_->uid;
  stbuf_->st_gid   = attr_->gid;
  stbuf_->st_size  = attr_->size;
  stbuf_->st_atime = attr_->atime;
  stbuf_->st_mtime = attr_->mtime;
  stbuf_->st_ctime = attr_->ctime;
  ST_ATIM_NSEC_SET(stbuf_,attr_->atimensec);
  ST_MTIM_NSEC_SET(stbuf_,attr_->mtimensec);
  ST_CTIM_NSEC_SET(stbuf_,attr_->ctimensec);
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
  if (req == NULL)
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
  struct fuse_out_header *out = iov[0].iov_base;

  out->len = iov_length(iov, count);

  return fuse_chan_send(ch, iov, count);
}

int
fuse_send_reply_iov_nofree(fuse_req_t    req,
                           int           error,
                           struct iovec *iov,
                           int           count)
{
  struct fuse_out_header out;

  if (error <= -1000 || error > 0)
    {
      fprintf(stderr, "fuse: bad error value: %i\n",error);
      error = -ERANGE;
    }

  out.unique = req->unique;
  out.error = error;

  iov[0].iov_base = &out;
  iov[0].iov_len = sizeof(struct fuse_out_header);

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
  if (argsize)
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
  return send_reply(req_,-err_,NULL,0);
}

void
fuse_reply_none(fuse_req_t req)
{
  if (req->ch)
    fuse_chan_send(req->ch, NULL, 0);
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
fill_open(struct fuse_open_out   *arg,
          const fuse_file_info_t *f)
{
  arg->fh = f->fh;
  if (f->direct_io)
    arg->open_flags |= FOPEN_DIRECT_IO;
  if (f->keep_cache)
    arg->open_flags |= FOPEN_KEEP_CACHE;
  if (f->nonseekable)
    arg->open_flags |= FOPEN_NONSEEKABLE;
  if (f->cache_readdir)
    arg->open_flags |= FOPEN_CACHE_DIR;
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
  if (!e->ino && req->f->conn.proto_minor < 4)
    return fuse_reply_err(req, ENOENT);

  fill_entry(&arg, e);

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
  void *mbuf;
  struct fuse_bufvec mem_buf = FUSE_BUFVEC_INIT(len);

  /* Optimize common case */
  if (buf->count == 1 && buf->idx == 0 && buf->off == 0 &&
      !(buf->buf[0].flags & FUSE_BUF_IS_FD))
    {
      /* FIXME: also avoid memory copy if there are multiple buffers
         but none of them contain an fd */

      iov[iov_count].iov_base = buf->buf[0].mem;
      iov[iov_count].iov_len = len;
      iov_count++;

      return fuse_send_msg(f, ch, iov, iov_count);
    }

  res = posix_memalign(&mbuf, pagesize, len);
  if (res != 0)
    return res;

  mem_buf.buf[0].mem = mbuf;
  res = fuse_buf_copy(&mem_buf, buf, 0);
  if (res < 0)
    {
      free(mbuf);
      return -res;
    }
  len = res;

  iov[iov_count].iov_base = mbuf;
  iov[iov_count].iov_len = len;
  iov_count++;
  res = fuse_send_msg(f, ch, iov, iov_count);
  free(mbuf);

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

#ifdef HAVE_SPLICE
static
struct fuse_ll_pipe*
fuse_ll_get_pipe(struct fuse_ll *f)
{
  struct fuse_ll_pipe *llp = pthread_getspecific(f->pipe_key);
  if (llp == NULL)
    {
      int res;

      llp = malloc(sizeof(struct fuse_ll_pipe));
      if (llp == NULL)
        return NULL;

      res = pipe(llp->pipe);
      if (res == -1)
        {
          free(llp);
          return NULL;
        }

      if (fcntl(llp->pipe[0], F_SETFL, O_NONBLOCK) == -1 ||
          fcntl(llp->pipe[1], F_SETFL, O_NONBLOCK) == -1)
        {
          close(llp->pipe[0]);
          close(llp->pipe[1]);
          free(llp);
          return NULL;
        }

      /*
       *the default size is 16 pages on linux
       */
      llp->size = pagesize * 16;
      llp->can_grow = 1;

      pthread_setspecific(f->pipe_key, llp);
    }

  return llp;
}
#endif

static
void
fuse_ll_clear_pipe(struct fuse_ll *f)
{
  struct fuse_ll_pipe *llp = pthread_getspecific(f->pipe_key);

  if (llp)
    {
      pthread_setspecific(f->pipe_key, NULL);
      fuse_ll_pipe_free(llp);
    }
}

#if defined(HAVE_SPLICE) && defined(HAVE_VMSPLICE)
static
int
read_back(int     fd,
          char   *buf,
          size_t  len)
{
  int res;

  res = read(fd, buf, len);
  if (res == -1)
    {
      fprintf(stderr, "fuse: internal error: failed to read back from pipe: %s\n", strerror(errno));
      return -EIO;
    }

  if (res != len)
    {
      fprintf(stderr, "fuse: internal error: short read back from pipe: %i from %zi\n", res, len);
      return -EIO;
    }

  return 0;
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
  int res;
  size_t len = fuse_buf_size(buf);
  struct fuse_out_header *out = iov[0].iov_base;
  struct fuse_ll_pipe *llp;
  int splice_flags;
  size_t pipesize;
  size_t total_fd_size;
  size_t idx;
  size_t headerlen;
  struct fuse_bufvec pipe_buf = FUSE_BUFVEC_INIT(len);

  if (f->broken_splice_nonblock)
    goto fallback;

  if (flags & FUSE_BUF_NO_SPLICE)
    goto fallback;

  total_fd_size = 0;
  for (idx = buf->idx; idx < buf->count; idx++)
    {
      if (buf->buf[idx].flags & FUSE_BUF_IS_FD)
        {
          total_fd_size = buf->buf[idx].size;
          if (idx == buf->idx)
            total_fd_size -= buf->off;
        }
    }

  if (total_fd_size < 2 * pagesize)
    goto fallback;

  if (f->conn.proto_minor < 14 || !(f->conn.want & FUSE_CAP_SPLICE_WRITE))
    goto fallback;

  llp = fuse_ll_get_pipe(f);
  if (llp == NULL)
    goto fallback;

  headerlen = iov_length(iov, iov_count);

  out->len = headerlen + len;

  /*
   * Heuristic for the required pipe size, does not work if the
   * source contains less than page size fragments
   */
  pipesize = pagesize * (iov_count + buf->count + 1) + out->len;

  if (llp->size < pipesize)
    {
      if (llp->can_grow)
        {
          res = fcntl(llp->pipe[0], F_SETPIPE_SZ, pipesize);
          if (res == -1)
            {
              llp->can_grow = 0;
              goto fallback;
            }
          llp->size = res;
        }

      if (llp->size < pipesize)
        goto fallback;
    }

  res = vmsplice(llp->pipe[1], iov, iov_count, SPLICE_F_NONBLOCK);
  if (res == -1)
    goto fallback;

  if (res != headerlen)
    {
      res = -EIO;
      fprintf(stderr, "fuse: short vmsplice to pipe: %u/%zu\n", res,
              headerlen);
      goto clear_pipe;
    }

  pipe_buf.buf[0].flags = FUSE_BUF_IS_FD;
  pipe_buf.buf[0].fd = llp->pipe[1];

  res = fuse_buf_copy(&pipe_buf, buf,
                      FUSE_BUF_FORCE_SPLICE | FUSE_BUF_SPLICE_NONBLOCK);
  if (res < 0)
    {
      if (res == -EAGAIN || res == -EINVAL)
        {
          /*
           * Should only get EAGAIN on kernels with
           * broken SPLICE_F_NONBLOCK support (<=
           * 2.6.35) where this error or a short read is
           * returned even if the pipe itself is not
           * full
           *
           * EINVAL might mean that splice can't handle
           * this combination of input and output.
           */
          if (res == -EAGAIN)
            f->broken_splice_nonblock = 1;

          pthread_setspecific(f->pipe_key, NULL);
          fuse_ll_pipe_free(llp);
          goto fallback;
        }
      res = -res;
      goto clear_pipe;
    }

  if (res != 0 && res < len)
    {
      struct fuse_bufvec mem_buf = FUSE_BUFVEC_INIT(len);
      void *mbuf;
      size_t now_len = res;
      /*
       * For regular files a short count is either
       *  1) due to EOF, or
       *  2) because of broken SPLICE_F_NONBLOCK (see above)
       *
       * For other inputs it's possible that we overflowed
       * the pipe because of small buffer fragments.
       */

      res = posix_memalign(&mbuf, pagesize, len);
      if (res != 0)
        goto clear_pipe;

      mem_buf.buf[0].mem = mbuf;
      mem_buf.off = now_len;
      res = fuse_buf_copy(&mem_buf, buf, 0);
      if (res > 0)
        {
          char *tmpbuf;
          size_t extra_len = res;
          /*
           * Trickiest case: got more data.  Need to get
           * back the data from the pipe and then fall
           * back to regular write.
           */
          tmpbuf = malloc(headerlen);
          if (tmpbuf == NULL)
            {
              free(mbuf);
              res = ENOMEM;
              goto clear_pipe;
            }
          res = read_back(llp->pipe[0], tmpbuf, headerlen);
          free(tmpbuf);
          if (res != 0)
            {
              free(mbuf);
              goto clear_pipe;
            }
          res = read_back(llp->pipe[0], mbuf, now_len);
          if (res != 0)
            {
              free(mbuf);
              goto clear_pipe;
            }
          len = now_len + extra_len;
          iov[iov_count].iov_base = mbuf;
          iov[iov_count].iov_len = len;
          iov_count++;
          res = fuse_send_msg(f, ch, iov, iov_count);
          free(mbuf);
          return res;
        }
      free(mbuf);
      res = now_len;
    }
  len = res;
  out->len = headerlen + len;

  splice_flags = 0;
  if ((flags & FUSE_BUF_SPLICE_MOVE) &&
      (f->conn.want & FUSE_CAP_SPLICE_MOVE))
    splice_flags |= SPLICE_F_MOVE;

  res = splice(llp->pipe[0], NULL, fuse_chan_fd(ch), NULL, out->len, splice_flags);
  if (res == -1)
    {
      res = -errno;
      perror("fuse: splice from pipe");
      goto clear_pipe;
    }

  if (res != out->len)
    {
      res = -EIO;
      fprintf(stderr, "fuse: short splice from pipe: %u/%u\n",
              res, out->len);
      goto clear_pipe;
    }

  return 0;

 clear_pipe:
  fuse_ll_clear_pipe(f);
  return res;

 fallback:
  return fuse_send_data_iov_fallback(f, ch, iov, iov_count, buf, len);
}
#else
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
#endif

int
fuse_reply_data(fuse_req_t                req,
                struct fuse_bufvec       *bufv,
                enum fuse_buf_copy_flags  flags)
{
  struct iovec iov[2];
  struct fuse_out_header out;
  int res;

  iov[0].iov_base = &out;
  iov[0].iov_len = sizeof(struct fuse_out_header);

  out.unique = req->unique;
  out.error = 0;

  res = fuse_send_data_iov(req->f, req->ch, iov, 1, bufv, flags);
  if (res <= 0)
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
  if (lock->l_type != F_UNLCK)
    {
      arg.lk.start = lock->l_start;
      if (lock->l_len == 0)
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

  fiov = malloc(sizeof(fiov[0]) * count);
  if (!fiov)
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

  if (req->f->conn.proto_minor < 16)
    {
      if (in_count)
        {
          iov[count].iov_base = (void *)in_iov;
          iov[count].iov_len = sizeof(in_iov[0]) * in_count;
          count++;
        }

      if (out_count)
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

      if (in_count)
        {
          in_fiov = fuse_ioctl_iovec_copy(in_iov, in_count);
          if (!in_fiov)
            goto enomem;

          iov[count].iov_base = (void *)in_fiov;
          iov[count].iov_len = sizeof(in_fiov[0]) * in_count;
          count++;
        }
      if (out_count)
        {
          out_fiov = fuse_ioctl_iovec_copy(out_iov, out_count);
          if (!out_fiov)
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

  padded_iov = malloc((count + 2) * sizeof(struct iovec));
  if (padded_iov == NULL)
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
do_lookup(fuse_req_t  req,
          uint64_t    nodeid,
          const void *inarg)
{
  char *name = (char*)inarg;

  req->f->op.lookup(req,nodeid,name);
}

static
void
do_forget(fuse_req_t  req,
          uint64_t    nodeid,
          const void *inarg)
{
  struct fuse_forget_in *arg = (struct fuse_forget_in*)inarg;

  req->f->op.forget(req,nodeid,arg->nlookup);
}

static
void
do_batch_forget(fuse_req_t  req,
                uint64_t    nodeid,
                const void *inarg)
{
  struct fuse_batch_forget_in *arg = (void *) inarg;
  struct fuse_forget_one *param = (void *) PARAM(arg);

  (void)nodeid;

  req->f->op.forget_multi(req,
                          arg->count,
                          (struct fuse_forget_data*)param);
}

static
void
do_getattr(fuse_req_t  req,
           uint64_t    nodeid,
           const void *inarg)
{
  fuse_file_info_t *fip = NULL;
  fuse_file_info_t fi = {0};

  if(req->f->conn.proto_minor >= 9)
    {
      struct fuse_getattr_in *arg = (struct fuse_getattr_in*)inarg;

      if(arg->getattr_flags & FUSE_GETATTR_FH)
        {
          fi.fh = arg->fh;
          fip = &fi;
        }
    }

  req->f->op.getattr(req, nodeid, fip);
}

static
void
do_setattr(fuse_req_t  req_,
           uint64_t    nodeid_,
           const void *inarg_)
{
  struct stat stbuf = {0};
  fuse_file_info_t *fi;
  fuse_file_info_t fi_store;
  struct fuse_setattr_in *arg;

  fi = NULL;
  arg = (struct fuse_setattr_in*)inarg_;

  convert_attr(arg,&stbuf);

  if(arg->valid & FATTR_FH)
    {
      arg->valid &= ~FATTR_FH;
      memset(&fi_store,0,sizeof(fi_store));
      fi = &fi_store;
      fi->fh = arg->fh;
    }

  arg->valid &=
    (FATTR_MODE      |
     FATTR_UID       |
     FATTR_GID       |
     FATTR_SIZE      |
     FATTR_ATIME     |
     FATTR_MTIME     |
     FATTR_CTIME     |
     FATTR_ATIME_NOW |
     FATTR_MTIME_NOW);

  req_->f->op.setattr(req_,nodeid_,&stbuf,arg->valid,fi);
}

static
void
do_access(fuse_req_t  req,
          uint64_t    nodeid,
          const void *inarg)
{
  struct fuse_access_in *arg = (struct fuse_access_in *)inarg;

  req->f->op.access(req, nodeid, arg->mask);
}

static
void
do_readlink(fuse_req_t  req,
            uint64_t    nodeid,
            const void *inarg)
{
  (void)inarg;

  req->f->op.readlink(req, nodeid);
}

static
void
do_mknod(fuse_req_t  req,
         uint64_t    nodeid,
         const void *inarg)
{
  struct fuse_mknod_in *arg = (struct fuse_mknod_in *) inarg;
  char *name = PARAM(arg);

  if (req->f->conn.proto_minor >= 12)
    req->ctx.umask = arg->umask;
  else
    name = (char*)inarg + FUSE_COMPAT_MKNOD_IN_SIZE;

  req->f->op.mknod(req, nodeid, name, arg->mode, arg->rdev);
}

static
void
do_mkdir(fuse_req_t  req,
         uint64_t    nodeid,
         const void *inarg)
{
  struct fuse_mkdir_in *arg = (struct fuse_mkdir_in *) inarg;

  if(req->f->conn.proto_minor >= 12)
    req->ctx.umask = arg->umask;

  req->f->op.mkdir(req, nodeid, PARAM(arg), arg->mode);
}

static
void
do_unlink(fuse_req_t  req,
          uint64_t    nodeid,
          const void *inarg)
{
  char *name = (char*)inarg;

  req->f->op.unlink(req,nodeid,name);
}

static
void
do_rmdir(fuse_req_t  req,
         uint64_t    nodeid,
         const void *inarg)
{
  char *name = (char*)inarg;

  req->f->op.rmdir(req, nodeid, name);
}

static
void
do_symlink(fuse_req_t  req,
           uint64_t    nodeid,
           const void *inarg)
{
  char *name     = (char*)inarg;
  char *linkname = (name + strlen(name) + 1);

  req->f->op.symlink(req, linkname, nodeid, name);
}

static
void
do_rename(fuse_req_t  req,
          uint64_t    nodeid,
          const void *inarg)
{
  struct fuse_rename_in *arg = (struct fuse_rename_in*)inarg;
  char *oldname = PARAM(arg);
  char *newname = oldname + strlen(oldname) + 1;

  req->f->op.rename(req, nodeid, oldname, arg->newdir, newname);
}

static
void
do_link(fuse_req_t  req,
        uint64_t    nodeid,
        const void *inarg)
{
  struct fuse_link_in *arg = (struct fuse_link_in*)inarg;

  req->f->op.link(req,arg->oldnodeid,nodeid,PARAM(arg));
}

static
void
do_create(fuse_req_t  req,
          uint64_t    nodeid,
          const void *inarg)
{
  struct fuse_create_in *arg = (struct fuse_create_in*)inarg;
  fuse_file_info_t fi = {0};
  char *name = PARAM(arg);

  fi.flags = arg->flags;

  if (req->f->conn.proto_minor >= 12)
    req->ctx.umask = arg->umask;
  else
    name = (char*)inarg + sizeof(struct fuse_open_in);

  req->f->op.create(req, nodeid, name, arg->mode, &fi);
}

static
void
do_open(fuse_req_t  req,
        uint64_t    nodeid,
        const void *inarg)
{
  fuse_file_info_t fi = {0};
  struct fuse_open_in *arg = (struct fuse_open_in*)inarg;

  fi.flags = arg->flags;

  req->f->op.open(req, nodeid, &fi);
}

static
void
do_read(fuse_req_t  req,
        uint64_t    nodeid,
        const void *inarg)
{
  fuse_file_info_t fi = {0};
  struct fuse_read_in *arg = (struct fuse_read_in*)inarg;

  fi.fh = arg->fh;
  if (req->f->conn.proto_minor >= 9)
    {
      fi.lock_owner = arg->lock_owner;
      fi.flags = arg->flags;
    }

  req->f->op.read(req, nodeid, arg->size, arg->offset, &fi);
}

static
void
do_write(fuse_req_t  req,
         uint64_t    nodeid,
         const void *inarg)
{
  char *param;
  fuse_file_info_t fi = {0};
  struct fuse_write_in *arg = (struct fuse_write_in*)inarg;

  fi.fh = arg->fh;
  fi.writepage = arg->write_flags & 1;

  if(req->f->conn.proto_minor < 9)
    {
      param = ((char*)arg) + FUSE_COMPAT_WRITE_IN_SIZE;
    }
  else
    {
      fi.lock_owner = arg->lock_owner;
      fi.flags = arg->flags;
      param = PARAM(arg);
    }

  req->f->op.write(req,nodeid,param,arg->size,arg->offset,&fi);
}

static
void
do_flush(fuse_req_t  req,
         uint64_t    nodeid,
         const void *inarg)
{
  fuse_file_info_t fi = {0};
  struct fuse_flush_in *arg = (struct fuse_flush_in *) inarg;

  fi.fh = arg->fh;
  fi.flush = 1;
  if(req->f->conn.proto_minor >= 7)
    fi.lock_owner = arg->lock_owner;

  req->f->op.flush(req,nodeid,&fi);
}

static
void
do_release(fuse_req_t  req,
           uint64_t    nodeid,
           const void *inarg)
{
  fuse_file_info_t fi = {0};
  struct fuse_release_in *arg = (struct fuse_release_in*)inarg;

  fi.flags = arg->flags;
  fi.fh = arg->fh;
  if(req->f->conn.proto_minor >= 8)
    {
      fi.flush = (arg->release_flags & FUSE_RELEASE_FLUSH) ? 1 : 0;
      fi.lock_owner = arg->lock_owner;
    }

  if(arg->release_flags & FUSE_RELEASE_FLOCK_UNLOCK)
    {
      fi.flock_release = 1;
      fi.lock_owner = arg->lock_owner;
    }

  req->f->op.release(req,nodeid,&fi);
}

static
void
do_fsync(fuse_req_t  req,
         uint64_t    nodeid,
         const void *inarg)
{
  fuse_file_info_t fi = {0};
  struct fuse_fsync_in *arg = (struct fuse_fsync_in*)inarg;

  fi.fh = arg->fh;

  req->f->op.fsync(req,nodeid,arg->fsync_flags & 1, &fi);
}

static
void
do_opendir(fuse_req_t  req,
           uint64_t    nodeid,
           const void *inarg)
{
  fuse_file_info_t fi = {0};
  struct fuse_open_in *arg = (struct fuse_open_in*)inarg;

  fi.flags = arg->flags;

  req->f->op.opendir(req,nodeid,&fi);
}

static
void
do_readdir(fuse_req_t  req,
           uint64_t    nodeid,
           const void *inarg)
{
  fuse_file_info_t fi = {0};
  struct fuse_read_in *arg = (struct fuse_read_in*)inarg;

  fi.fh = arg->fh;

  req->f->op.readdir(req,nodeid,arg->size,arg->offset,&fi);
}

static
void
do_readdir_plus(fuse_req_t  req_,
                uint64_t    nodeid_,
                const void *inarg_)
{
  const struct fuse_read_in *arg;
  fuse_file_info_t ffi = {0};

  arg    = (struct fuse_read_in*)inarg_;
  ffi.fh = arg->fh;

  req_->f->op.readdir_plus(req_,nodeid_,arg->size,arg->offset,&ffi);
}

static
void
do_releasedir(fuse_req_t  req,
              uint64_t    nodeid,
              const void *inarg)
{
  fuse_file_info_t fi = {0};
  struct fuse_release_in *arg = (struct fuse_release_in*)inarg;

  fi.flags = arg->flags;
  fi.fh = arg->fh;

  req->f->op.releasedir(req,nodeid,&fi);
}

static
void
do_fsyncdir(fuse_req_t  req,
            uint64_t    nodeid,
            const void *inarg)
{
  fuse_file_info_t fi = {0};
  struct fuse_fsync_in *arg = (struct fuse_fsync_in*)inarg;

  fi.fh = arg->fh;

  req->f->op.fsyncdir(req,nodeid,arg->fsync_flags & 1,&fi);
}

static
void
do_statfs(fuse_req_t  req,
          uint64_t    nodeid,
          const void *inarg)
{
  (void)nodeid;
  (void)inarg;

  req->f->op.statfs(req, nodeid);
}

static
void
do_setxattr(fuse_req_t  req,
            uint64_t    nodeid,
            const void *inarg)
{
  struct fuse_setxattr_in *arg = (struct fuse_setxattr_in*)inarg;
  char *name  = PARAM(arg);
  char *value = name + strlen(name) + 1;

  req->f->op.setxattr(req, nodeid, name, value, arg->size, arg->flags);
}

static
void
do_getxattr(fuse_req_t  req,
            uint64_t    nodeid,
            const void *inarg)
{
  struct fuse_getxattr_in *arg = (struct fuse_getxattr_in*)inarg;

  req->f->op.getxattr(req, nodeid, PARAM(arg), arg->size);
}

static
void
do_listxattr(fuse_req_t  req,
             uint64_t    nodeid,
             const void *inarg)
{
  struct fuse_getxattr_in *arg = (struct fuse_getxattr_in*)inarg;

  req->f->op.listxattr(req, nodeid, arg->size);
}

static
void
do_removexattr(fuse_req_t  req,
               uint64_t    nodeid,
               const void *inarg)
{
  char *name = (char *) inarg;

  req->f->op.removexattr(req, nodeid, name);
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
  if (fl->end == OFFSET_MAX)
    flock->l_len = 0;
  else
    flock->l_len = fl->end - fl->start + 1;
  flock->l_pid = fl->pid;
}

static
void
do_getlk(fuse_req_t  req,
         uint64_t    nodeid,
         const void *inarg)
{
  fuse_file_info_t fi = {0};
  struct flock flock;
  struct fuse_lk_in *arg = (struct fuse_lk_in*)inarg;

  fi.fh         = arg->fh;
  fi.lock_owner = arg->owner;

  convert_fuse_file_lock(&arg->lk, &flock);

  req->f->op.getlk(req, nodeid, &fi, &flock);
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
  struct fuse_lk_in *arg = (struct fuse_lk_in *) inarg;

  fi.fh = arg->fh;
  fi.lock_owner = arg->owner;

  if (arg->lk_flags & FUSE_LK_FLOCK)
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

      if (!sleep)
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
do_setlk(fuse_req_t  req,
         uint64_t    nodeid,
         const void *inarg)
{
  do_setlk_common(req, nodeid, inarg, 0);
}

static
void
do_setlkw(fuse_req_t  req,
          uint64_t    nodeid,
          const void *inarg)
{
  do_setlk_common(req, nodeid, inarg, 1);
}

static
void
do_interrupt(fuse_req_t  req,
             uint64_t    nodeid,
             const void *inarg)
{
  destroy_req(req);
}

static
void
do_bmap(fuse_req_t  req,
        uint64_t    nodeid,
        const void *inarg)
{
  struct fuse_bmap_in *arg = (struct fuse_bmap_in*)inarg;

  req->f->op.bmap(req,nodeid,arg->blocksize,arg->block);
}

static
void
do_ioctl(fuse_req_t  req,
         uint64_t    nodeid,
         const void *inarg)
{
  fuse_file_info_t fi = {0};
  struct fuse_ioctl_in *arg = (struct fuse_ioctl_in *) inarg;
  unsigned int flags = arg->flags;
  void *in_buf = arg->in_size ? PARAM(arg) : NULL;

  if((flags & FUSE_IOCTL_DIR) && !(req->f->conn.want & FUSE_CAP_IOCTL_DIR))
    {
      fuse_reply_err(req,ENOTTY);
      return;
    }

  fi.fh = arg->fh;

  if((sizeof(void *) == 4) &&
     (req->f->conn.proto_minor >= 16) &&
     !(flags & FUSE_IOCTL_32BIT))
    {
      req->ioctl_64bit = 1;
    }

  req->f->op.ioctl(req, nodeid, (unsigned long)arg->cmd,
                   (void *)(uintptr_t)arg->arg, &fi, flags,
                   in_buf, arg->in_size, arg->out_size);
}

void
fuse_pollhandle_destroy(fuse_pollhandle_t *ph)
{
  free(ph);
}

static
void
do_poll(fuse_req_t  req,
        uint64_t    nodeid,
        const void *inarg)
{
  fuse_file_info_t fi = {0};
  fuse_pollhandle_t *ph = NULL;
  struct fuse_poll_in *arg = (struct fuse_poll_in *) inarg;

  fi.fh = arg->fh;

  if(arg->flags & FUSE_POLL_SCHEDULE_NOTIFY)
    {
      ph = malloc(sizeof(fuse_pollhandle_t));
      if (ph == NULL) {
        fuse_reply_err(req, ENOMEM);
        return;
      }
      ph->kh = arg->kh;
      ph->ch = req->ch;
      ph->f = req->f;
    }

  req->f->op.poll(req,nodeid,&fi,ph);
}

static
void
do_fallocate(fuse_req_t  req,
             uint64_t    nodeid,
             const void *inarg)
{
  fuse_file_info_t fi = {0};
  struct fuse_fallocate_in *arg = (struct fuse_fallocate_in *) inarg;

  fi.fh = arg->fh;

  req->f->op.fallocate(req, nodeid, arg->mode, arg->offset, arg->length, &fi);
}

static
void
do_init(fuse_req_t  req,
        uint64_t    nodeid,
        const void *inarg)
{
  struct fuse_init_out outarg = {0};
  struct fuse_init_in *arg = (struct fuse_init_in *) inarg;
  struct fuse_ll *f = req->f;
  size_t bufsize = fuse_chan_bufsize(req->ch);

  (void)nodeid;

  if(f->debug)
    debug_fuse_init_in(arg);

  f->conn.proto_major = arg->major;
  f->conn.proto_minor = arg->minor;
  f->conn.capable = 0;
  f->conn.want = 0;

  outarg.major     = FUSE_KERNEL_VERSION;
  outarg.minor     = FUSE_KERNEL_MINOR_VERSION;
  outarg.max_pages = FUSE_DEFAULT_MAX_PAGES_PER_REQ;

  if (arg->major < 7)
    {
      fprintf(stderr, "fuse: unsupported protocol version: %u.%u\n",
              arg->major, arg->minor);
      fuse_reply_err(req, EPROTO);
      return;
    }

  if (arg->major > 7)
    {
      /* Wait for a second INIT request with a 7.X version */
      send_reply_ok(req, &outarg, sizeof(outarg));
      return;
    }

  if (arg->minor >= 6)
    {
      if (arg->max_readahead < f->conn.max_readahead)
        f->conn.max_readahead = arg->max_readahead;
      if (arg->flags & FUSE_ASYNC_READ)
        f->conn.capable |= FUSE_CAP_ASYNC_READ;
      if (arg->flags & FUSE_POSIX_LOCKS)
        f->conn.capable |= FUSE_CAP_POSIX_LOCKS;
      if (arg->flags & FUSE_ATOMIC_O_TRUNC)
        f->conn.capable |= FUSE_CAP_ATOMIC_O_TRUNC;
      if (arg->flags & FUSE_EXPORT_SUPPORT)
        f->conn.capable |= FUSE_CAP_EXPORT_SUPPORT;
      if (arg->flags & FUSE_BIG_WRITES)
        f->conn.capable |= FUSE_CAP_BIG_WRITES;
      if (arg->flags & FUSE_DONT_MASK)
        f->conn.capable |= FUSE_CAP_DONT_MASK;
      if (arg->flags & FUSE_FLOCK_LOCKS)
        f->conn.capable |= FUSE_CAP_FLOCK_LOCKS;
      if (arg->flags & FUSE_POSIX_ACL)
        f->conn.capable |= FUSE_CAP_POSIX_ACL;
      if (arg->flags & FUSE_CACHE_SYMLINKS)
        f->conn.capable |= FUSE_CAP_CACHE_SYMLINKS;
      if (arg->flags & FUSE_ASYNC_DIO)
        f->conn.capable |= FUSE_CAP_ASYNC_DIO;
      if (arg->flags & FUSE_PARALLEL_DIROPS)
        f->conn.capable |= FUSE_CAP_PARALLEL_DIROPS;
      if (arg->flags & FUSE_MAX_PAGES)
        f->conn.capable |= FUSE_CAP_MAX_PAGES;
      if (arg->flags & FUSE_WRITEBACK_CACHE)
        f->conn.capable |= FUSE_CAP_WRITEBACK_CACHE;
      if (arg->flags & FUSE_DO_READDIRPLUS)
        f->conn.capable |= FUSE_CAP_READDIR_PLUS;
      if (arg->flags & FUSE_READDIRPLUS_AUTO)
        f->conn.capable |= FUSE_CAP_READDIR_PLUS_AUTO;
    }
  else
    {
      f->conn.want &= ~FUSE_CAP_ASYNC_READ;
      f->conn.max_readahead = 0;
    }

  if (req->f->conn.proto_minor >= 14)
    {
#ifdef HAVE_SPLICE
#ifdef HAVE_VMSPLICE
      f->conn.capable |= FUSE_CAP_SPLICE_WRITE | FUSE_CAP_SPLICE_MOVE;
      if (f->splice_write)
        f->conn.want |= FUSE_CAP_SPLICE_WRITE;
      if (f->splice_move)
        f->conn.want |= FUSE_CAP_SPLICE_MOVE;
#endif
      f->conn.capable |= FUSE_CAP_SPLICE_READ;
      if (f->splice_read)
        f->conn.want |= FUSE_CAP_SPLICE_READ;
#endif
    }

  if (req->f->conn.proto_minor >= 18)
    f->conn.capable |= FUSE_CAP_IOCTL_DIR;

  if (f->op.getlk && f->op.setlk && !f->no_remote_posix_lock)
    f->conn.want |= FUSE_CAP_POSIX_LOCKS;
  if (f->op.flock && !f->no_remote_flock)
    f->conn.want |= FUSE_CAP_FLOCK_LOCKS;

  if (bufsize < FUSE_MIN_READ_BUFFER)
    {
      fprintf(stderr, "fuse: warning: buffer size too small: %zu\n",
              bufsize);
      bufsize = FUSE_MIN_READ_BUFFER;
    }

  bufsize -= 4096;
  if (bufsize < f->conn.max_write)
    f->conn.max_write = bufsize;

  f->got_init = 1;
  if (f->op.init)
    f->op.init(f->userdata, &f->conn);

  if (f->no_splice_read)
    f->conn.want &= ~FUSE_CAP_SPLICE_READ;
  if (f->no_splice_write)
    f->conn.want &= ~FUSE_CAP_SPLICE_WRITE;
  if (f->no_splice_move)
    f->conn.want &= ~FUSE_CAP_SPLICE_MOVE;

  if ((arg->flags & FUSE_MAX_PAGES) && (f->conn.want & FUSE_CAP_MAX_PAGES))
    {
      outarg.flags     |= FUSE_MAX_PAGES;
      outarg.max_pages  = f->conn.max_pages;
    }

  if (f->conn.want & FUSE_CAP_ASYNC_READ)
    outarg.flags |= FUSE_ASYNC_READ;
  if (f->conn.want & FUSE_CAP_POSIX_LOCKS)
    outarg.flags |= FUSE_POSIX_LOCKS;
  if (f->conn.want & FUSE_CAP_ATOMIC_O_TRUNC)
    outarg.flags |= FUSE_ATOMIC_O_TRUNC;
  if (f->conn.want & FUSE_CAP_EXPORT_SUPPORT)
    outarg.flags |= FUSE_EXPORT_SUPPORT;
  if (f->conn.want & FUSE_CAP_BIG_WRITES)
    outarg.flags |= FUSE_BIG_WRITES;
  if (f->conn.want & FUSE_CAP_DONT_MASK)
    outarg.flags |= FUSE_DONT_MASK;
  if (f->conn.want & FUSE_CAP_FLOCK_LOCKS)
    outarg.flags |= FUSE_FLOCK_LOCKS;
  if (f->conn.want & FUSE_CAP_POSIX_ACL)
    outarg.flags |= FUSE_POSIX_ACL;
  if (f->conn.want & FUSE_CAP_CACHE_SYMLINKS)
    outarg.flags |= FUSE_CACHE_SYMLINKS;
  if (f->conn.want & FUSE_CAP_ASYNC_DIO)
    outarg.flags |= FUSE_ASYNC_DIO;
  if (f->conn.want & FUSE_CAP_PARALLEL_DIROPS)
    outarg.flags |= FUSE_PARALLEL_DIROPS;
  if (f->conn.want & FUSE_CAP_WRITEBACK_CACHE)
    outarg.flags |= FUSE_WRITEBACK_CACHE;
  if (f->conn.want & FUSE_CAP_READDIR_PLUS)
    outarg.flags |= FUSE_DO_READDIRPLUS;
  if (f->conn.want & FUSE_CAP_READDIR_PLUS_AUTO)
    outarg.flags |= FUSE_READDIRPLUS_AUTO;

  outarg.max_readahead = f->conn.max_readahead;
  outarg.max_write = f->conn.max_write;
  if (f->conn.proto_minor >= 13)
    {
      if (f->conn.max_background >= (1 << 16))
        f->conn.max_background = (1 << 16) - 1;
      if (f->conn.congestion_threshold > f->conn.max_background)
        f->conn.congestion_threshold = f->conn.max_background;
      if (!f->conn.congestion_threshold)
        {
          f->conn.congestion_threshold = f->conn.max_background * 3 / 4;
        }

      outarg.max_background = f->conn.max_background;
      outarg.congestion_threshold = f->conn.congestion_threshold;
    }

  size_t outargsize;
  if(arg->minor < 5)
    outargsize = FUSE_COMPAT_INIT_OUT_SIZE;
  else if(arg->minor < 23)
    outargsize = FUSE_COMPAT_22_INIT_OUT_SIZE;
  else
    outargsize = sizeof(outarg);

  if(f->debug)
    debug_fuse_init_out(req->unique,&outarg,outargsize);

  send_reply_ok(req, &outarg, outargsize);
}

static
void
do_destroy(fuse_req_t  req,
           uint64_t    nodeid,
           const void *inarg)
{
  struct fuse_ll *f = req->f;

  (void) nodeid;
  (void) inarg;

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
do_notify_reply(fuse_req_t  req,
                uint64_t    nodeid,
                const void *inarg)
{
  struct fuse_ll *f = req->f;
  struct fuse_notify_req *nreq;
  struct fuse_notify_req *head;

  pthread_mutex_lock(&f->lock);
  head = &f->notify_list;
  for(nreq = head->next; nreq != head; nreq = nreq->next)
    {
      if(nreq->unique == req->unique)
        {
          list_del_nreq(nreq);
          break;
        }
    }
  pthread_mutex_unlock(&f->lock);

  if(nreq != head)
    nreq->reply(nreq, req, nodeid, inarg);
}

static
void
do_copy_file_range(fuse_req_t  req_,
                   uint64_t  nodeid_in_,
                   const void *arg_)

{
  fuse_file_info_t ffi_in  = {0};
  fuse_file_info_t ffi_out = {0};
  struct fuse_copy_file_range_in *arg = (struct fuse_copy_file_range_in*)arg_;

  ffi_in.fh  = arg->fh_in;
  ffi_out.fh = arg->fh_out;

  req_->f->op.copy_file_range(req_,
                              nodeid_in_,
                              arg->off_in,
                              &ffi_in,
                              arg->nodeid_out,
                              arg->off_out,
                              &ffi_out,
                              arg->len,
                              arg->flags);
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

  if (!ch)
    return -EINVAL;

  f = (struct fuse_ll *)fuse_session_data(fuse_chan_session(ch));
  if (!f)
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
                                 uint64_t        parent,
                                 const char       *name,
                                 size_t            namelen)
{
  struct fuse_notify_inval_entry_out outarg;
  struct fuse_ll *f;
  struct iovec iov[3];

  if (!ch)
    return -EINVAL;

  f = (struct fuse_ll *)fuse_session_data(fuse_chan_session(ch));
  if (!f)
    return -ENODEV;

  outarg.parent = parent;
  outarg.namelen = namelen;
  outarg.padding = 0;

  iov[1].iov_base = &outarg;
  iov[1].iov_len = sizeof(outarg);
  iov[2].iov_base = (void *)name;
  iov[2].iov_len = namelen + 1;

  return send_notify_iov(f, ch, FUSE_NOTIFY_INVAL_ENTRY, iov, 3);
}

int
fuse_lowlevel_notify_delete(struct fuse_chan *ch,
                            uint64_t        parent,
                            uint64_t        child,
                            const char       *name,
                            size_t            namelen)
{
  struct fuse_notify_delete_out outarg;
  struct fuse_ll *f;
  struct iovec iov[3];

  if (!ch)
    return -EINVAL;

  f = (struct fuse_ll *)fuse_session_data(fuse_chan_session(ch));
  if (!f)
    return -ENODEV;

  if (f->conn.proto_minor < 18)
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
                           uint64_t                ino,
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

  if (!ch)
    return -EINVAL;

  f = (struct fuse_ll *)fuse_session_data(fuse_chan_session(ch));
  if (!f)
    return -ENODEV;

  if (f->conn.proto_minor < 15)
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
  if (res > 0)
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

  if (!ch)
    return -EINVAL;

  f = (struct fuse_ll*)fuse_session_data(fuse_chan_session(ch));
  if(!f)
    return -ENODEV;

  if(f->conn.proto_minor < 15)
    return -ENOSYS;

  rreq = malloc(sizeof(*rreq));
  if(rreq == NULL)
    return -ENOMEM;

  pthread_mutex_lock(&f->lock);
  rreq->cookie = cookie;
  rreq->nreq.unique = f->notify_ctr++;
  rreq->nreq.reply = fuse_ll_retrieve_reply;
  list_add_nreq(&rreq->nreq, &f->notify_list);
  pthread_mutex_unlock(&f->lock);

  outarg.notify_unique = rreq->nreq.unique;
  outarg.nodeid = ino;
  outarg.offset = offset;
  outarg.size = size;

  iov[1].iov_base = &outarg;
  iov[1].iov_len = sizeof(outarg);

  err = send_notify_iov(f, ch, FUSE_NOTIFY_RETRIEVE, iov, 2);
  if(err)
    {
      pthread_mutex_lock(&f->lock);
      list_del_nreq(&rreq->nreq);
      pthread_mutex_unlock(&f->lock);
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

static struct {
  void (*func)(fuse_req_t, uint64_t, const void *);
  const char *name;
} fuse_ll_ops[] =
  {
   [FUSE_LOOKUP]          = { do_lookup,          "LOOKUP"	    },
   [FUSE_FORGET]          = { do_forget,          "FORGET"	    },
   [FUSE_GETATTR]         = { do_getattr,         "GETATTR"         },
   [FUSE_SETATTR]         = { do_setattr,         "SETATTR"         },
   [FUSE_READLINK]        = { do_readlink,        "READLINK"        },
   [FUSE_SYMLINK]         = { do_symlink,         "SYMLINK"         },
   [FUSE_MKNOD]           = { do_mknod,           "MKNOD"	    },
   [FUSE_MKDIR]           = { do_mkdir,           "MKDIR"	    },
   [FUSE_UNLINK]          = { do_unlink,          "UNLINK"	    },
   [FUSE_RMDIR]           = { do_rmdir,           "RMDIR"	    },
   [FUSE_RENAME]          = { do_rename,          "RENAME"	    },
   [FUSE_LINK]            = { do_link,	          "LINK"	    },
   [FUSE_OPEN]            = { do_open,	          "OPEN"	    },
   [FUSE_READ]            = { do_read,	          "READ"	    },
   [FUSE_WRITE]           = { do_write,           "WRITE"	    },
   [FUSE_STATFS]          = { do_statfs,          "STATFS"	    },
   [FUSE_RELEASE]         = { do_release,         "RELEASE"         },
   [FUSE_FSYNC]           = { do_fsync,           "FSYNC"	    },
   [FUSE_SETXATTR]        = { do_setxattr,        "SETXATTR"        },
   [FUSE_GETXATTR]        = { do_getxattr,        "GETXATTR"        },
   [FUSE_LISTXATTR]       = { do_listxattr,       "LISTXATTR"       },
   [FUSE_REMOVEXATTR]     = { do_removexattr,     "REMOVEXATTR"     },
   [FUSE_FLUSH]           = { do_flush,           "FLUSH"	    },
   [FUSE_INIT]            = { do_init,	          "INIT"	    },
   [FUSE_OPENDIR]         = { do_opendir,         "OPENDIR"         },
   [FUSE_READDIR]         = { do_readdir,         "READDIR"         },
   [FUSE_READDIRPLUS]     = { do_readdir_plus,    "READDIR_PLUS"    },
   [FUSE_RELEASEDIR]      = { do_releasedir,      "RELEASEDIR"      },
   [FUSE_FSYNCDIR]        = { do_fsyncdir,        "FSYNCDIR"        },
   [FUSE_GETLK]           = { do_getlk,           "GETLK"	    },
   [FUSE_SETLK]           = { do_setlk,           "SETLK"	    },
   [FUSE_SETLKW]          = { do_setlkw,          "SETLKW"	    },
   [FUSE_ACCESS]          = { do_access,          "ACCESS"	    },
   [FUSE_CREATE]          = { do_create,          "CREATE"	    },
   [FUSE_INTERRUPT]       = { do_interrupt,       "INTERRUPT"       },
   [FUSE_BMAP]            = { do_bmap,	          "BMAP"	    },
   [FUSE_IOCTL]           = { do_ioctl,           "IOCTL"	    },
   [FUSE_POLL]            = { do_poll,            "POLL"	    },
   [FUSE_FALLOCATE]       = { do_fallocate,       "FALLOCATE"       },
   [FUSE_DESTROY]         = { do_destroy,         "DESTROY"         },
   [FUSE_NOTIFY_REPLY]    = { do_notify_reply,    "NOTIFY_REPLY"    },
   [FUSE_BATCH_FORGET]    = { do_batch_forget,    "BATCH_FORGET"    },
   [FUSE_COPY_FILE_RANGE] = { do_copy_file_range, "COPY_FILE_RANGE" },
  };

#define FUSE_MAXOP (sizeof(fuse_ll_ops) / sizeof(fuse_ll_ops[0]))

static
void
fuse_ll_process_buf(void                  *data,
                    const struct fuse_buf *buf,
                    struct fuse_chan      *ch)
{
  struct fuse_ll *f = (struct fuse_ll*)data;
  struct fuse_in_header *in;
  const void *inarg;
  struct fuse_req *req;
  int err;

  in = buf->mem;

  req = fuse_ll_alloc_req(f);
  if(req == NULL)
    {
      struct fuse_out_header out = {
        .unique = in->unique,
        .error = -ENOMEM,
      };
      struct iovec iov = {
        .iov_base = &out,
        .iov_len = sizeof(struct fuse_out_header),
      };

      fuse_send_msg(f, ch, &iov, 1);
      return;
    }

  req->unique = in->unique;
  req->ctx.uid = in->uid;
  req->ctx.gid = in->gid;
  req->ctx.pid = in->pid;
  req->ch = ch;

  err = EIO;
  if(!f->got_init)
    {
      enum fuse_opcode expected;

      expected = FUSE_INIT;
      if (in->opcode != expected)
        goto reply_err;
    }
  else if(in->opcode == FUSE_INIT)
    {
      goto reply_err;
    }

  err = ENOSYS;
  if(in->opcode >= FUSE_MAXOP)
    goto reply_err;

  inarg = (void*)&in[1];
  fuse_ll_ops[in->opcode].func(req, in->nodeid, inarg);

  return;

 reply_err:
  fuse_reply_err(req, err);
  return;
}

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
   { "splice_write", offsetof(struct fuse_ll, splice_write), 1},
   { "no_splice_write", offsetof(struct fuse_ll, no_splice_write), 1},
   { "splice_move", offsetof(struct fuse_ll, splice_move), 1},
   { "no_splice_move", offsetof(struct fuse_ll, no_splice_move), 1},
   { "splice_read", offsetof(struct fuse_ll, splice_read), 1},
   { "no_splice_read", offsetof(struct fuse_ll, no_splice_read), 1},
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
          "    -o [no_]splice_write   use splice to write to the fuse device\n"
          "    -o [no_]splice_move    move data while splicing to the fuse device\n"
          "    -o [no_]splice_read    use splice to read from the fuse device\n"
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
  struct fuse_ll *f = (struct fuse_ll *) data;
  struct fuse_ll_pipe *llp;

  if (f->got_init && !f->got_destroy)
    {
      if (f->op.destroy)
        f->op.destroy(f->userdata);
    }

  llp = pthread_getspecific(f->pipe_key);
  if (llp != NULL)
    fuse_ll_pipe_free(llp);
  pthread_key_delete(f->pipe_key);
  pthread_mutex_destroy(&f->lock);
  free(f);

  lfmp_clear(&g_FMP_fuse_req);
}

static
void
fuse_ll_pipe_destructor(void *data)
{
  struct fuse_ll_pipe *llp = data;
  fuse_ll_pipe_free(llp);
}

#ifdef HAVE_SPLICE
static
int
fuse_ll_receive_buf(struct fuse_session *se,
                    struct fuse_buf     *buf)
{
  struct fuse_ll *f = fuse_session_data(se);
  size_t bufsize = buf->size;
  struct fuse_ll_pipe *llp;
  int err;
  int res;

  if(f->conn.proto_minor < 14 || !(f->conn.want & FUSE_CAP_SPLICE_READ))
    goto fallback;

  llp = fuse_ll_get_pipe(f);
  if (llp == NULL)
    goto fallback;

  if(llp->size < bufsize)
    {
      if(llp->can_grow)
        {
          res = fcntl(llp->pipe[0], F_SETPIPE_SZ, bufsize);
          if(res == -1)
            {
              llp->can_grow = 0;
              goto fallback;
            }
          llp->size = res;
        }
      if(llp->size < bufsize)
        goto fallback;
    }

  res = splice(fuse_chan_fd(se->ch), NULL, llp->pipe[1], NULL, bufsize, SPLICE_F_MOVE);
  err = errno;

  if(fuse_session_exited(se))
    return 0;

  if(res == -1)
    {
      if(err == ENODEV)
        {
          fuse_session_exit(se);
          return 0;
        }

      if(err != EINTR && err != EAGAIN)
        perror("fuse: splice from device");

      return -err;
    }

  if(res < sizeof(struct fuse_in_header))
    {
      fprintf(stderr, "short splice from fuse device\n");
      return -EIO;
    }

  struct iovec iov = { buf->mem, res };
  res = vmsplice(llp->pipe[0], &iov, 1, 0);

  return res;

 fallback:
  res = fuse_chan_recv(se->ch, buf->mem, buf->size);
  if(res <= 0)
    return res;

  buf->size = res;

  return res;
}
#else
static
int
fuse_ll_receive_buf(struct fuse_session *se,
                    struct fuse_buf     *buf)
{
  int res;

  res = fuse_chan_recv(se->ch, buf->mem, buf->size);
  if(res <= 0)
    return res;

  buf->size = res;

  return res;
}
#endif

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

  if (sizeof(struct fuse_lowlevel_ops) < op_size)
    {
      fprintf(stderr, "fuse: warning: library too old, some operations may not work\n");
      op_size = sizeof(struct fuse_lowlevel_ops);
    }

  f = (struct fuse_ll *) calloc(1, sizeof(struct fuse_ll));
  if (f == NULL)
    {
      fprintf(stderr, "fuse: failed to allocate fuse object\n");
      goto out;
    }

  f->conn.max_write = UINT_MAX;
  f->conn.max_readahead = UINT_MAX;
  list_init_nreq(&f->notify_list);
  f->notify_ctr = 1;
  fuse_mutex_init(&f->lock);

  err = pthread_key_create(&f->pipe_key, fuse_ll_pipe_destructor);
  if (err)
    {
      fprintf(stderr, "fuse: failed to create thread specific key: %s\n",
              strerror(err));
      goto out_free;
    }

  if (fuse_opt_parse(args, f, fuse_ll_opts, fuse_ll_opt_proc) == -1)
    goto out_key_destroy;

  memcpy(&f->op, op, op_size);
  f->owner = getuid();
  f->userdata = userdata;

  se = fuse_session_new(f,
                        fuse_ll_receive_buf,
                        fuse_ll_process_buf,
                        fuse_ll_destroy);
  if (!se)
    goto out_key_destroy;

  return se;

 out_key_destroy:
  pthread_key_delete(f->pipe_key);
 out_free:
  pthread_mutex_destroy(&f->lock);
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
