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

#include "debug.hpp"
#include "fatal.hpp"
#include "fmt/core.h"
#include "fuse_cfg.hpp"
#include "fuse_i.hpp"
#include "fuse_kernel.h"
#include "fuse_msgbuf.hpp"
#include "fuse_opt.h"
#include "fuse_pollhandle.h"
#include "stat_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#define PARAM(inarg) (((char*)(inarg)) + sizeof(*(inarg)))
#define OFFSET_MAX 0x7fffffffffffffffLL

#define container_of(ptr, type, member) ({                      \
      const decltype( ((type*)0)->member ) *__mptr = (ptr);     \
      (type *)( (char*)__mptr - offsetof(type,member) );})

static size_t pagesize;

struct fuse_ll
{
  struct fuse_lowlevel_ops op;
  uid_t owner;
  fuse_conn_info_t conn;
  mutex_t lock;
  int got_init;
  int got_destroy;
  uint64_t notify_ctr;
  struct fuse_notify_req notify_list;
};

static fuse_ll f = {};

static
__attribute__((constructor))
void
fuse_ll_constructor(void)
{
  pagesize = sysconf(_SC_PAGESIZE);
  if(pagesize <= 0)
    fatal::abort("pagesize query failed - {}",strerror(errno));
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
int
fuse_send_msg(const int     fd_,
              struct iovec *iov,
              int           count)
{
  int rv;
  struct fuse_out_header *out = (fuse_out_header*)iov[0].iov_base;

  out->len = iov_length(iov, count);

  rv = writev(fd_,iov,count);
  if(rv == -1)
    return -errno;

  return 0;
}

/* ---------------------------------------------------------------------------
 * splice fallback logging
 *
 * Every path that silently degrades splice behavior (pipe sizing
 * fallbacks, sysctl read failures, splice(2) errors) logs once per
 * process so operators can see why splice went quiet, without spamming
 * per-request logs on a busy mount. The logger itself (and the pipe
 * sizing helpers below) live in fuse_msgbuf.cpp/.hpp, shared with the
 * receive-side pipe plumbing there.
 * ------------------------------------------------------------------------- */
#include "syslog.hpp"

#include <array>

#define MAX_ERRNO 4095

int
fuse_send_reply_iov_nofree(fuse_req_t   *req,
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

  out.unique = req->ctx.unique;
  out.error  = error;

  iov[0].iov_base = &out;
  iov[0].iov_len  = sizeof(struct fuse_out_header);

  return fuse_send_msg(req->fd, iov, count);
}

static
int
send_reply_iov(fuse_req_t   *req,
               int           error,
               struct iovec *iov,
               int           count)
{
  int res;

  res = fuse_send_reply_iov_nofree(req, error, iov, count);
  fuse_req_free(req);

  return res;
}

static
int
send_reply(fuse_req_t *req,
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
send_reply_ok(fuse_req_t *req,
              const void *arg,
              size_t      argsize)
{
  return send_reply(req, 0, arg, argsize);
}

int
fuse_reply_err(fuse_req_t *req_,
               int         err_)
{
  if(fuse_cfg.debug)
    {
      struct fuse_out_header hdr = {};
      hdr.unique = req_->ctx.unique;
      hdr.error  = (err_ > 0) ? -err_ : err_;
      hdr.len    = sizeof(struct fuse_out_header);
      fuse_debug_out_header(&hdr);
    }

  return send_reply(req_,err_,NULL,0);
}

void
fuse_reply_none(fuse_req_t *req)
{
  fuse_req_free(req);
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
  if(ffi_->passthrough && fuse_backing_id_is_valid(ffi_->backing_id))
    {
      arg_->open_flags |= FOPEN_PASSTHROUGH;
      arg_->backing_id  = ffi_->backing_id;
    }
}

int
fuse_reply_entry(fuse_req_t                    *req,
                 const struct fuse_entry_param *e)
{
  struct fuse_entry_out arg = {};
  size_t size = req->conn.proto_minor < 9 ?
    FUSE_COMPAT_ENTRY_OUT_SIZE : sizeof(arg);

  /* before ABI 7.4 e->ino == 0 was invalid, only ENOENT meant
     negative entry */
  if(!e->ino && req->conn.proto_minor < 4)
    return fuse_reply_err(req, ENOENT);

  fill_entry(&arg, e);

#ifdef NDEBUG
  // TODO: Add checks for cases where a node could be marked bad by
  // the kernel.
#endif

  if(fuse_cfg.debug)
    fuse_debug_entry_out(req->ctx.unique,&arg,size);

  return send_reply_ok(req, &arg, size);
}

struct fuse_create_out
{
  struct fuse_entry_out e;
  struct fuse_open_out o;
};

int
fuse_reply_create(fuse_req_t                    *req,
                  const struct fuse_entry_param *e,
                  const fuse_file_info_t        *f)
{
  struct fuse_create_out buf = {};
  size_t entrysize = req->conn.proto_minor < 9 ?
    FUSE_COMPAT_ENTRY_OUT_SIZE : sizeof(struct fuse_entry_out);
  struct fuse_entry_out *earg = (struct fuse_entry_out*)&buf.e;
  struct fuse_open_out  *oarg = (struct fuse_open_out*)(((char*)&buf)+entrysize);

  fill_entry(earg, e);
  fill_open(oarg, f);

  if(fuse_cfg.debug)
    fuse_debug_entry_open_out(req->ctx.unique,earg,oarg);

  return send_reply_ok(req, &buf, entrysize + sizeof(struct fuse_open_out));
}

int
fuse_reply_attr(fuse_req_t        *req,
                const struct stat *attr,
                const uint64_t     timeout)
{
  struct fuse_attr_out arg = {};
  size_t size = req->conn.proto_minor < 9 ?
    FUSE_COMPAT_ATTR_OUT_SIZE : sizeof(arg);

  arg.attr_valid      = timeout;
  arg.attr_valid_nsec = 0;
  convert_stat(attr,&arg.attr);

  if(fuse_cfg.debug)
    fuse_debug_attr_out(req->ctx.unique,&arg,size);

  return send_reply_ok(req,&arg,size);
}

int
fuse_reply_statx(fuse_req_t        *req_,
                 int                flags_,
                 struct fuse_statx *st_,
                 uint64_t           timeout_)
{
  struct fuse_statx_out outarg{};

  outarg.flags           = flags_;
  outarg.attr_valid      = timeout_;
  outarg.attr_valid_nsec = 0;
  outarg.stat            = *(struct fuse_statx*)st_;

  if(fuse_cfg.debug)
    fuse_debug_statx_out(req_->ctx.unique,&outarg);

  return send_reply_ok(req_,&outarg,sizeof(outarg));
}

int
fuse_reply_readlink(fuse_req_t   *req,
                    const char   *linkname,
                    const size_t  linkname_len)
{
  if(not linkname)
    return fuse_reply_err(req,EIO);

  if(fuse_cfg.debug)
    fuse_debug_readlink(req->ctx.unique,linkname,linkname_len);

  return send_reply_ok(req,linkname,linkname_len);
}

int
fuse_reply_open(fuse_req_t             *req,
                const fuse_file_info_t *f)
{
  struct fuse_open_out arg = {};

  fill_open(&arg, f);

  if(fuse_cfg.debug)
    fuse_debug_open_out(req->ctx.unique,&arg,sizeof(arg));

  return send_reply_ok(req, &arg, sizeof(arg));
}

int
fuse_reply_write(fuse_req_t *req,
                 size_t      count)
{
  struct fuse_write_out arg = {};

  arg.size = count;

  if(fuse_cfg.debug)
    fuse_debug_write_out(req->ctx.unique,&arg);

  return send_reply_ok(req, &arg, sizeof(arg));
}

int
fuse_reply_copy_file_range_64(fuse_req_t *req,
                              size_t      count)
{
  struct fuse_copy_file_range_out arg = {};

  arg.bytes_copied = count;

  return send_reply_ok(req, &arg, sizeof(arg));
}

int
fuse_reply_buf(fuse_req_t *req,
               const char *buf,
               size_t      size)
{
  if(fuse_cfg.debug)
    fuse_debug_data_out(req->ctx.unique,size);

  return send_reply_ok(req, buf, size);
}

/*
 * Kernel 5.x removed vmsplice() of user pages into /dev/fuse directly
 * (EBADF): vmsplice targets a pipe, always has. Use the classic
 * pipe-bounce: vmsplice header+data pages into a per-thread pipe, then
 * splice(2) that pipe into the fuse device so the kernel sees a single
 * contiguous reply message.
 *
 * Thread-local pipe so read-reply pages are shared with the splice
 * receive worker that created them; lazily sized to pipe-max.
 */

/* Thread-local reply bounce pipe. Wrapped in a tiny RAII struct
   (rather than a bare thread_local int[2] + a free "close" function)
   so its destructor runs automatically at thread exit and closes both
   fds: a POD array has no such hook, and the previous free-function
   equivalent of this destructor had zero call sites anywhere in the
   tree, silently leaking 2 fds per worker thread for the life of the
   process on every mount that ever serviced a >=128KiB read reply or
   a splice-move reply.

   ensure/ready/drain/vmspliceIn/spliceOut are methods on the struct
   itself, rather than free functions taking no arguments and reaching
   into the g_reply_pipe global implicitly - same shape as Mutex/
   LockGuard (mutex.hpp) and FileInfo (src/fileinfo.hpp) elsewhere in
   this codebase. */
struct _ReplyPipe
{
  int fd[2] = {-1,-1};
  u32 cap   = 0;

  ~_ReplyPipe()
  {
    if(fd[0] != -1) ::close(fd[0]);
    if(fd[1] != -1) ::close(fd[1]);
  }

  int     ensure();
  bool    ready(size_t needed, size_t slots);
  void    drain();
  ssize_t vmspliceIn(struct iovec *iov, int iovcnt);
  ssize_t spliceOut(int dst_fd, size_t total);
};

static thread_local _ReplyPipe g_reply_pipe;

/* Everything below through _fuse_reply_data_splice_move uses
   vmsplice(2)/splice(2)/SPLICE_F_MOVE/SPLICE_F_NONBLOCK/F_SETPIPE_SZ,
   all Linux-only. On other platforms only the public
   fuse_reply_data_splice_fd entry point needs to exist (fuse.cpp
   calls it unconditionally); it always reports "nothing committed,
   fall back" so callers transparently use the portable read/write
   path, matching the documented behavior ("splice remains
   unavailable on platforms that lack it; the copy path is the
   fallback everywhere"). */
#if defined(__linux__)

int
_ReplyPipe::ensure()
{
  if(fd[0] != -1)
    return 0;

  int rv = ::pipe2(fd,O_CLOEXEC);
  if(rv == -1)
    {
      fd[0] = -1;
      fd[1] = -1;
      return -errno;
    }

  /* size to whatever the kernel will let an unprivileged user have.
     pipe-max-size read and F_SETPIPE_SZ-with-retry are shared with
     msgbuf_ensure_pipe (fuse_msgbuf.cpp) so the two can't disagree
     about the pipe's real capacity. */
  {
    u32 pipe_max = fuse_effective_pipe_max();

    /* F_SETPIPE_SZ failure is non-fatal here (unlike msgbuf_ensure_pipe):
       pipe just stays default-size and splice will short-write, which
       ready()'s cap check below already handles. */
    rv = fuse_pipe_set_size(fd[0],pipe_max,/*retry_unconditionally_=*/true,
                            splice_fallback_tag_t::REPLY_PIPE_FSETPIPE_ANY,
                            splice_fallback_tag_t::REPLY_PIPE_FSETPIPE_EP);
    cap = (rv > 0) ? (u32)rv : 65536;
  }

  return 0;
}

void
_ReplyPipe::drain()
{
  /* Discard whatever is still queued in the bounce pipe after an
     error mid-reply: without this, the NEXT fuse_reply_data on this
     thread would prepend stale bytes to its fresh one, corrupting the
     wire stream. */
  std::array<char,65536> buf;

  fuse_drain_pipe_fd(fd[0],buf.data(),buf.size());
}

/* vmsplice iov (header + payload) into the pipe. Returns -errno
 * only when nothing was moved. */
ssize_t
_ReplyPipe::vmspliceIn(struct iovec *iov_,
                       int           iovcnt_)
{
  size_t done = 0;
  size_t want = iov_length(iov_,iovcnt_);

  while(done < want)
    {
      ssize_t res = vmsplice(fd[1],iov_,iovcnt_,SPLICE_F_MOVE|SPLICE_F_NONBLOCK);
      if(res == -1)
        {
          int e = errno;
          if(e == EINTR)
            continue;
          if(done == 0)
            return -e;
          return -EIO;
        }
      if(res == 0)
        return -EIO;
      done += res;
      /* advance iov */
      {
        size_t left = res;
        int    i = 0;
        for(; i < iovcnt_ && left > 0; )
          {
            if(iov_[i].iov_len <= left)
              {
                left -= iov_[i].iov_len;
                i++;
              }
            else
              {
                iov_[i].iov_base  = (char*)iov_[i].iov_base + left;
                iov_[i].iov_len  -= left;
                left = 0;
              }
          }
        if(i > 0)
          {
            memmove(&iov_[0],&iov_[i],(iovcnt_ - i) * sizeof(struct iovec));
            iovcnt_ -= i;
          }
      }
    }

  return (ssize_t)done;
}

/* Drain the pipe into the fuse device. Returns -errno only when nothing
 * was moved to the device; once any bytes land, they form the start of
 * an atomic reply message and the caller must NOT retry with writev. */
ssize_t
_ReplyPipe::spliceOut(const int dst_fd_,
                      size_t    total_)
{
  size_t done = 0;

  while(done < total_)
    {
      ssize_t res = ::splice(fd[0],nullptr,
                             dst_fd_,nullptr,
                             total_ - done,
                             SPLICE_F_MOVE);
      if(res == -1)
        {
          int e = errno;
          if(e == EINTR)
            continue;
          if(done == 0)
            return -e;
          return -EIO;
        }
      if(res == 0)
        {
          /* pipe empty but caller expected more - shouldn't happen since
             we vmspliced exactly _total_. */
          return -EIO;
        }
      done += res;
    }

  return (ssize_t)done;
}

/* Splice payload from a source fd (branch file) into the reply pipe,
 * header first via vmsplice. Wire is untouched until the final
 * pipe->/dev/fuse splice, so any failure before that is
 * fallback-safe.
 *
 * Public splice-from-fd reply. Return semantics:
 *  0   success, req consumed (freed), caller does NOT fuse_req_free
 *  -1  nothing committed to the wire; caller must fall back to another
 *      reply mechanism (or pread) and free req there
 * -EIO wire committed a partial message; caller just frees req
 */
static
int
_fuse_reply_data_splice_fd(fuse_req_t   *req_,
                           const int     src_fd_,
                           const off_t   pos_,
                           size_t        size_);

int
fuse_reply_data_splice_fd(fuse_req_t    *req_,
                          const int      src_fd_,
                          const off_t    pos_,
                          const size_t   size_)
{
  int rv = _fuse_reply_data_splice_fd(req_,src_fd_,pos_,size_);
  if(rv == -1)
    return -1;

  fuse_req_free(req_);

  return rv;
}

/* Number of pipe ring slots a single source segment consumes. A pipe's
   capacity is enforced in whole page-sized slots, not bytes: each page
   touched by a segment - including a partial first and last page -
   becomes its own pipe buffer. */
static
size_t
_pipe_slots(const uintptr_t base_,
            const size_t    len_)
{
  size_t ps = (pagesize > 0) ? pagesize : 4096;

  if(len_ == 0)
    return 0;

  return (((base_ % ps) + len_ + ps - 1) / ps);
}

/* Shared by both reply-splice variants below: ensure the thread-local
   reply pipe exists and can hold needed_ bytes in slots_ ring slots,
   logging the same two fallback tags either variant would have logged
   inline. */
bool
_ReplyPipe::ready(size_t needed_,
                  size_t slots_)
{
  if(ensure() != 0)
    {
      fuse_splice_fallback_log(splice_fallback_tag_t::REPLY_PIPE_ENSURE);
      return false;
    }

  /* Pipe capacity guard: we need header + payload to fit, else splice
     would block with no reader on /dev/fuse advanced. cap is sized to
     max pipe-max >= msgbuf but we verify explicitly. The byte check
     alone is not sufficient: the kernel limits a pipe to cap/pagesize
     slots and every partial page (the 16 byte header especially)
     burns a whole one, so a byte-wise fitting message can still fill
     the ring and block forever. */
  if(needed_ > cap)
    {
      fuse_splice_fallback_log(splice_fallback_tag_t::REPLY_PIPE_CAP);
      return false;
    }

  if(slots_ > (cap / ((pagesize > 0) ? pagesize : 4096)))
    {
      fuse_splice_fallback_log(splice_fallback_tag_t::REPLY_PIPE_CAP);
      return false;
    }

  return true;
}

/* Shared by both reply-splice variants below: a partial write to
   /dev/fuse after any bytes reached the wire poisons the connection
   (unlike every other, milder fallback condition in these functions),
   so - unlike those - it's always logged, not just once per process. */
static
void
_reply_splice_desync_log(const char *variant_)
{
  SysLog::error("splice reply: partial write to /dev/fuse"
                " ({}); connection may be desynced",variant_);
  fprintf(stderr,"mergerfs: splice reply: partial write to /dev/fuse"
          " (%s); connection may be desynced\n",variant_);
}

static
int
_fuse_reply_data_splice_fd(fuse_req_t   *req_,
                           const int     src_fd_,
                           const off_t   pos_,
                           size_t        size_)
{
  struct fuse_out_header out;
  struct iovec iov[1];

  out.unique = req_->ctx.unique;
  out.error  = 0;
  out.len    = (u32)(sizeof(out) + size_);

  if(fuse_cfg.debug)
    fuse_debug_data_out(req_->ctx.unique,size_);

  size_t total = sizeof(out) + size_;
  size_t slots = (_pipe_slots((uintptr_t)&out,sizeof(out)) +
                  _pipe_slots((uintptr_t)pos_,size_));
  if(!g_reply_pipe.ready(total,slots))
    return -1;

  iov[0].iov_base = &out;
  iov[0].iov_len  = sizeof(out);

  ssize_t rh = g_reply_pipe.vmspliceIn(iov,1);
  if(rh != (ssize_t)sizeof(out))
    {
      fuse_splice_fallback_log(splice_fallback_tag_t::REPLY_VMSPLICE_HDR);
      g_reply_pipe.drain();
      return -1;
    }


  /* fd -> pipe: kernel-internal copy from page cache into pipe ring.
     Short return = EOF or a transient failure; either way the wire is
     still clean and the caller can fall back to a plain pread. */
  {
    size_t done = 0;

    while(done < size_)
      {
        off_t off = pos_ + (off_t)done;

        /* SPLICE_F_NONBLOCK applies to the pipe end only (the file read
           still blocks as needed): if the slot accounting in ready()
           ever under-estimates, this returns EAGAIN and we degrade to
           the copy path instead of blocking forever with no reader. */
        ssize_t res = ::splice(src_fd_,&off,
                               g_reply_pipe.fd[1],nullptr,
                               size_ - done,
                               SPLICE_F_MOVE|SPLICE_F_NONBLOCK);
        if(res == -1)
          {
            if(errno == EINTR)
              continue;
            fuse_splice_fallback_log(splice_fallback_tag_t::REPLY_FD_SPLICE);
            g_reply_pipe.drain();
            return -1;
          }
        if(res == 0)
          {
            /* short read at EOF: drain partial payload and let the
               caller reprobe with pread so ERR/EOF padding matches
               the normal path. */
            fuse_splice_fallback_log(splice_fallback_tag_t::REPLY_FD_SPLICE);
            g_reply_pipe.drain();
            return -1;
          }
        done += res;
      }
  }

  ssize_t sent = g_reply_pipe.spliceOut(req_->fd,total);
  if(sent == -EIO)
    {
      /* partial wire send: the request is poisoned, but any bytes
         still queued in the bounce pipe must be drained or the next
         reply on this thread would be prefixed with stale bytes. */
      _reply_splice_desync_log("fd-splice read reply");
      g_reply_pipe.drain();
      return -EIO;
    }
  if(sent < 0)
    {
      /* nothing reached the wire; the wire is clean and the caller
         may fall back to pread. Contract is {0,-1,-EIO} - never
         propagate a raw -errno (the public wrapper frees the req for
         any rv != -1, and callers only guard 0/-EIO/-1). */
      g_reply_pipe.drain();
      return -1;
    }

  return 0;
}

static
int
_fuse_reply_data_splice_move(fuse_req_t   *req,
                             struct iovec *iov_,
                             int           iovcnt_)
{
  size_t want  = iov_length(iov_,iovcnt_);
  size_t slots = 0;
  for(int i = 0; i < iovcnt_; i++)
    slots += _pipe_slots((uintptr_t)iov_[i].iov_base,iov_[i].iov_len);
  if(!g_reply_pipe.ready(want,slots))
    return -1;

  ssize_t total = g_reply_pipe.vmspliceIn(iov_,iovcnt_);
  if(total < 0)
    {
      g_reply_pipe.drain();
      return -1;
    }
  if((size_t)total != want)
    {
      g_reply_pipe.drain();
      return -EIO;
    }

  ssize_t n = g_reply_pipe.spliceOut(req->fd,(size_t)total);
  if(n < 0)
    {
      g_reply_pipe.drain();
      /* -EIO when a partial message reached the wire (poisoned req);
         anything else means nothing moved - caller may fall back to
         writev. Never a raw -errno: the fallthrough caller only
         distinguishes 0 / -EIO / fallback. */
      if(n == -EIO)
        _reply_splice_desync_log("splice-move reply");
      return (n == -EIO) ? -EIO : -1;
    }
  if((size_t)n != (size_t)total)
    {
      g_reply_pipe.drain();
      return -EIO;
    }

  /* All sent; reply complete from kernel's point of view. */
  return 0;
}

#else /* !defined(__linux__) */

int
fuse_reply_data_splice_fd(fuse_req_t    *req_,
                          const int      src_fd_,
                          const off_t    pos_,
                          const size_t   size_)
{
  (void)req_; (void)src_fd_; (void)pos_; (void)size_;

  /* -1: nothing committed to the wire; caller falls back to a plain
     pread + fuse_reply_data. */
  return -1;
}

#endif /* defined(__linux__) */

int
fuse_reply_data(fuse_req_t   *req,
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

  out.unique = req->ctx.unique;
  out.error  = 0;
  /* fuse_send_msg computes out->len from iov_len on entry; when we take
     the pipe-bounce path we bypass it, so set len here too. */
  out.len    = (u32)(sizeof(out) + bufsize_);

  if(fuse_cfg.debug)
    fuse_debug_data_out(req->ctx.unique,bufsize_);

  /* Pipe-bounce splice_move reply: page-aligned payload moved
     zero-copy into the kernel's reply queue. Gated at 128 KiB (the
     same threshold as the fd-splice reply path); sub-threshold
     replies stay on writev - measured: 4K payloads lose ~30% on the
     pipe path, 128K+ gains ~14-19%. Returns:
       0  - success (reply sent)
      -EIO - partial move/transmit: the request is poisoned, do NOT
            fall back to writev or the kernel will see a duplicated prefix
      <  other -errno - nothing moved; caller may fall back to writev
  */
#if defined(__linux__)
  if(bufsize_ >= FUSE_SPLICE_MIN_SIZE && fuse_cfg.splice_move)
    {
      /* _ReplyPipe::vmspliceIn destructively advances its iov on a
         partial vmsplice; the writev fallthrough below must see the
         ORIGINAL header+payload, so pass a copy, never our stack
         array. */
      struct iovec spiov[2] = {iov[0], iov[1]};

      res = _fuse_reply_data_splice_move(req,spiov,2);
      if(res == 0)
        {
          fuse_req_free(req);
          return 0;
        }
      if(res == -EIO)
        {
          fuse_req_free(req);
          return res;
        }
      fuse_splice_fallback_log(splice_fallback_tag_t::REPLY_SPLICE_MOVE_WRITEV);
      /* fall through to writev */
    }
#endif /* defined(__linux__) */

  res = fuse_send_msg(req->fd,iov,2);
  fuse_req_free(req);

  return res;
}

int
fuse_reply_statfs(fuse_req_t           *req,
                  const struct statvfs *stbuf)
{
  struct fuse_statfs_out arg = {};
  size_t size = req->conn.proto_minor < 4 ?
    FUSE_COMPAT_STATFS_SIZE : sizeof(arg);

  convert_statfs(stbuf, &arg.st);

  if(fuse_cfg.debug)
    fuse_debug_statfs_out(req->ctx.unique,&arg);

  return send_reply_ok(req, &arg, size);
}

int
fuse_reply_xattr(fuse_req_t *req,
                 size_t      count)
{
  struct fuse_getxattr_out arg = {};

  arg.size = count;

  if(fuse_cfg.debug)
    fuse_debug_getxattr_out(req->ctx.unique,&arg);

  return send_reply_ok(req, &arg, sizeof(arg));
}

int
fuse_reply_bmap(fuse_req_t *req,
                uint64_t    idx)
{
  struct fuse_bmap_out arg = {};

  arg.block = idx;

  if(fuse_cfg.debug)
    fuse_debug_bmap_out(req->ctx.unique,&arg);

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
fuse_reply_ioctl_retry(fuse_req_t         *req,
                       const struct iovec *in_iov,
                       size_t              in_count,
                       const struct iovec *out_iov,
                       size_t              out_count)
{
  int res;
  struct fuse_ioctl_out arg = {};
  struct fuse_ioctl_iovec *in_fiov = NULL;
  struct fuse_ioctl_iovec *out_fiov = NULL;
  struct iovec iov[4];
  size_t count = 1;

  arg.flags |= FUSE_IOCTL_RETRY;
  arg.in_iovs = in_count;
  arg.out_iovs = out_count;

  if(fuse_cfg.debug)
    fuse_debug_ioctl_out(req->ctx.unique,&arg);
  iov[count].iov_base = &arg;
  iov[count].iov_len = sizeof(arg);
  count++;

  if(req->conn.proto_minor < 16)
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
fuse_reply_ioctl(fuse_req_t *req,
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

  if(fuse_cfg.debug)
    fuse_debug_ioctl_out(req->ctx.unique,&arg);

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
fuse_reply_poll(fuse_req_t *req,
                unsigned    revents)
{
  struct fuse_poll_out arg = {};

  arg.revents = revents;

  if(fuse_cfg.debug)
    fuse_debug_poll_out(req->ctx.unique,&arg);

  return send_reply_ok(req, &arg, sizeof(arg));
}

static
void
do_lookup(fuse_req_t            *req,
          struct fuse_in_header *hdr_)
{
  f.op.lookup(req,hdr_);
}

static
void
do_forget(fuse_req_t            *req,
          struct fuse_in_header *hdr_)
{
  f.op.forget(req,hdr_);
}

static
void
do_batch_forget(fuse_req_t            *req,
                struct fuse_in_header *hdr_)
{
  f.op.forget_multi(req,hdr_);
}

static
void
do_getattr(fuse_req_t            *req,
           struct fuse_in_header *hdr_)
{
  f.op.getattr(req, hdr_);
}

static
void
do_setattr(fuse_req_t            *req_,
           struct fuse_in_header *hdr_)
{
  f.op.setattr(req_,hdr_);
}

static
void
do_access(fuse_req_t            *req,
          struct fuse_in_header *hdr_)
{
  f.op.access(req,hdr_);
}

static
void
do_readlink(fuse_req_t            *req,
            struct fuse_in_header *hdr_)
{
  f.op.readlink(req,hdr_);
}

static
void
do_mknod(fuse_req_t            *req,
         struct fuse_in_header *hdr_)
{
  f.op.mknod(req,hdr_);
}

static
void
do_mkdir(fuse_req_t            *req,
         struct fuse_in_header *hdr_)
{
  f.op.mkdir(req,hdr_);
}

static
void
do_unlink(fuse_req_t            *req,
          struct fuse_in_header *hdr_)
{
  f.op.unlink(req,hdr_);
}

static
void
do_rmdir(fuse_req_t            *req,
         struct fuse_in_header *hdr_)
{
  f.op.rmdir(req,hdr_);
}

static
void
do_symlink(fuse_req_t            *req,
           struct fuse_in_header *hdr_)
{
  f.op.symlink(req,hdr_);
}

static
void
do_rename(fuse_req_t            *req,
          struct fuse_in_header *hdr_)
{
  f.op.rename(req,hdr_);
}

static
void
do_link(fuse_req_t            *req,
        struct fuse_in_header *hdr_)
{
  f.op.link(req,hdr_);
}

static
void
do_create(fuse_req_t            *req,
          struct fuse_in_header *hdr_)
{
  f.op.create(req,hdr_);
}

static
void
do_open(fuse_req_t            *req,
        struct fuse_in_header *hdr_)
{
  f.op.open(req,hdr_);
}

static
void
do_read(fuse_req_t            *req,
        struct fuse_in_header *hdr_)
{
  f.op.read(req,hdr_);
}

static
void
do_write(fuse_req_t            *req,
         struct fuse_in_header *hdr_)
{
  f.op.write(req,hdr_);
}

static
void
do_flush(fuse_req_t            *req,
         struct fuse_in_header *hdr_)
{
  f.op.flush(req,hdr_);
}

static
void
do_release(fuse_req_t            *req,
           struct fuse_in_header *hdr_)
{
  f.op.release(req,hdr_);
}

static
void
do_fsync(fuse_req_t            *req,
         struct fuse_in_header *hdr_)
{
  f.op.fsync(req,hdr_);
}

static
void
do_opendir(fuse_req_t            *req,
           struct fuse_in_header *hdr_)
{
  f.op.opendir(req,hdr_);
}

static
void
do_readdir(fuse_req_t            *req,
           struct fuse_in_header *hdr_)
{
  f.op.readdir(req,hdr_);
}

static
void
do_readdirplus(fuse_req_t            *req_,
               struct fuse_in_header *hdr_)
{
  f.op.readdir_plus(req_,hdr_);
}

static
void
do_releasedir(fuse_req_t            *req,
              struct fuse_in_header *hdr_)
{
  f.op.releasedir(req,hdr_);
}

static
void
do_fsyncdir(fuse_req_t            *req,
            struct fuse_in_header *hdr_)
{
  f.op.fsyncdir(req,hdr_);
}

static
void
do_statfs(fuse_req_t            *req,
          struct fuse_in_header *hdr_)
{
  f.op.statfs(req,hdr_);
}

static
void
do_setxattr(fuse_req_t            *req,
            struct fuse_in_header *hdr_)
{
  f.op.setxattr(req,hdr_);
}

static
void
do_getxattr(fuse_req_t            *req,
            struct fuse_in_header *hdr_)
{
  f.op.getxattr(req,hdr_);
}

static
void
do_listxattr(fuse_req_t            *req,
             struct fuse_in_header *hdr_)
{
  f.op.listxattr(req,hdr_);
}

static
void
do_removexattr(fuse_req_t            *req,
               struct fuse_in_header *hdr_)
{
  f.op.removexattr(req,hdr_);
}

void
do_interrupt(fuse_req_t *req,
             struct fuse_in_header *hdr_)
{
  fuse_req_free(req);
}

static
void
do_bmap(fuse_req_t            *req,
        struct fuse_in_header *hdr_)
{
  f.op.bmap(req,hdr_);
}

static
void
do_ioctl(fuse_req_t *req,
         struct fuse_in_header *hdr_)
{
  f.op.ioctl(req, hdr_);
}

void
fuse_pollhandle_destroy(fuse_pollhandle_t *ph)
{
  free(ph);
}

static
void
do_poll(fuse_req_t            *req,
        struct fuse_in_header *hdr_)
{
  f.op.poll(req,hdr_);
}

static
void
do_fallocate(fuse_req_t            *req,
             struct fuse_in_header *hdr_)
{
  f.op.fallocate(req,hdr_);
}

static
void
do_init(fuse_req_t            *req,
        struct fuse_in_header *hdr_)
{
  struct fuse_init_out outarg = {};
  struct fuse_init_in *arg = (struct fuse_init_in *)&hdr_[1];
  size_t bufsize;
  uint64_t inargflags;
  uint64_t outargflags;
  u32 max_write;
  u32 max_readahead;

  max_write = UINT_MAX;
  max_readahead = UINT_MAX;
  bufsize = req->se->bufsize;

  inargflags = 0;
  outargflags = 0;

  fuse_syslog_fuse_init_in(arg);

  f.conn.proto_major = arg->major;
  f.conn.proto_minor = arg->minor;
  f.conn.capable = 0;
  f.conn.want = 0;

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

      if(arg->max_readahead < max_readahead)
        max_readahead = arg->max_readahead;

      if(inargflags & FUSE_ASYNC_READ)
        f.conn.capable |= FUSE_CAP_ASYNC_READ;
      if(inargflags & FUSE_ATOMIC_O_TRUNC)
        f.conn.capable |= FUSE_CAP_ATOMIC_O_TRUNC;
      if(inargflags & FUSE_EXPORT_SUPPORT)
        f.conn.capable |= FUSE_CAP_EXPORT_SUPPORT;
      if(inargflags & FUSE_BIG_WRITES)
        f.conn.capable |= FUSE_CAP_BIG_WRITES;
      if(inargflags & FUSE_DONT_MASK)
        f.conn.capable |= FUSE_CAP_DONT_MASK;
      if(inargflags & FUSE_SPLICE_WRITE)
        f.conn.capable |= FUSE_CAP_SPLICE_WRITE;
      if(inargflags & FUSE_SPLICE_MOVE)
        f.conn.capable |= FUSE_CAP_SPLICE_MOVE;
      if(inargflags & FUSE_SPLICE_READ)
        f.conn.capable |= FUSE_CAP_SPLICE_READ;
      if(inargflags & FUSE_POSIX_ACL)
        f.conn.capable |= FUSE_CAP_POSIX_ACL;
      if(inargflags & FUSE_CACHE_SYMLINKS)
        f.conn.capable |= FUSE_CAP_CACHE_SYMLINKS;
      if(inargflags & FUSE_ASYNC_DIO)
        f.conn.capable |= FUSE_CAP_ASYNC_DIO;
      if(inargflags & FUSE_PARALLEL_DIROPS)
        f.conn.capable |= FUSE_CAP_PARALLEL_DIROPS;
      if(inargflags & FUSE_MAX_PAGES)
        f.conn.capable |= FUSE_CAP_MAX_PAGES;
      if(inargflags & FUSE_WRITEBACK_CACHE)
        f.conn.capable |= FUSE_CAP_WRITEBACK_CACHE;
      if(inargflags & FUSE_DO_READDIRPLUS)
        f.conn.capable |= FUSE_CAP_READDIR_PLUS;
      if(inargflags & FUSE_READDIRPLUS_AUTO)
        f.conn.capable |= FUSE_CAP_READDIR_PLUS_AUTO;
      if(inargflags & FUSE_SETXATTR_EXT)
        f.conn.capable |= FUSE_CAP_SETXATTR_EXT;
      if(inargflags & FUSE_DIRECT_IO_ALLOW_MMAP)
        f.conn.capable |= FUSE_CAP_DIRECT_IO_ALLOW_MMAP;
      if(inargflags & FUSE_CREATE_SUPP_GROUP)
        f.conn.capable |= FUSE_CAP_CREATE_SUPP_GROUP;
      if(inargflags & FUSE_PASSTHROUGH)
        f.conn.capable |= FUSE_CAP_PASSTHROUGH;
      if(inargflags & FUSE_HANDLE_KILLPRIV)
        f.conn.capable |= FUSE_CAP_HANDLE_KILLPRIV;
      if(inargflags & FUSE_HANDLE_KILLPRIV_V2)
        f.conn.capable |= FUSE_CAP_HANDLE_KILLPRIV_V2;
      if(inargflags & FUSE_ALLOW_IDMAP)
        f.conn.capable |= FUSE_CAP_ALLOW_IDMAP;
    }
  else
    {
      f.conn.want &= ~FUSE_CAP_ASYNC_READ;
      max_readahead = 0;
    }

  if(f.conn.proto_minor >= 18)
    f.conn.capable |= FUSE_CAP_IOCTL_DIR;

  if(bufsize < FUSE_MIN_READ_BUFFER)
    {
      fprintf(stderr, "fuse: warning: buffer size too small: %zu\n",
              bufsize);
      bufsize = FUSE_MIN_READ_BUFFER;
    }

  bufsize -= pagesize;
  if(bufsize < max_write)
    max_write = bufsize;

  f.got_init = 1;
  f.op.init(&f.conn);

  outargflags = outarg.flags;
  if((inargflags & FUSE_MAX_PAGES) && (f.conn.want & FUSE_CAP_MAX_PAGES))
    {
      outargflags      |= FUSE_MAX_PAGES;
      outarg.max_pages  = fuse_cfg.max_pages;

      msgbuf_set_bufsize(outarg.max_pages);
      max_write = (msgbuf_get_pagesize() * outarg.max_pages);
    }

  if(f.conn.want & FUSE_CAP_ASYNC_READ)
    outargflags |= FUSE_ASYNC_READ;
  if(f.conn.want & FUSE_CAP_ATOMIC_O_TRUNC)
    outargflags |= FUSE_ATOMIC_O_TRUNC;
  if(f.conn.want & FUSE_CAP_EXPORT_SUPPORT)
    outargflags |= FUSE_EXPORT_SUPPORT;
  if(f.conn.want & FUSE_CAP_BIG_WRITES)
    outargflags |= FUSE_BIG_WRITES;
  if(f.conn.want & FUSE_CAP_DONT_MASK)
    outargflags |= FUSE_DONT_MASK;
  if(f.conn.want & FUSE_CAP_SPLICE_WRITE)
    outargflags |= FUSE_SPLICE_WRITE;
  if(f.conn.want & FUSE_CAP_SPLICE_MOVE)
    outargflags |= FUSE_SPLICE_MOVE;
  if(f.conn.want & FUSE_CAP_SPLICE_READ)
    outargflags |= FUSE_SPLICE_READ;
  if(f.conn.want & FUSE_CAP_POSIX_ACL)
    outargflags |= FUSE_POSIX_ACL;
  if(f.conn.want & FUSE_CAP_CACHE_SYMLINKS)
    outargflags |= FUSE_CACHE_SYMLINKS;
  if(f.conn.want & FUSE_CAP_ASYNC_DIO)
    outargflags |= FUSE_ASYNC_DIO;
  if(f.conn.want & FUSE_CAP_PARALLEL_DIROPS)
    outargflags |= FUSE_PARALLEL_DIROPS;
  if(f.conn.want & FUSE_CAP_WRITEBACK_CACHE)
    outargflags |= FUSE_WRITEBACK_CACHE;
  if(f.conn.want & FUSE_CAP_READDIR_PLUS)
    outargflags |= FUSE_DO_READDIRPLUS;
  if(f.conn.want & FUSE_CAP_READDIR_PLUS_AUTO)
    outargflags |= FUSE_READDIRPLUS_AUTO;
  if(f.conn.want & FUSE_CAP_SETXATTR_EXT)
    outargflags |= FUSE_SETXATTR_EXT;
  if(f.conn.want & FUSE_CAP_CREATE_SUPP_GROUP)
    outargflags |= FUSE_CREATE_SUPP_GROUP;
  if(f.conn.want & FUSE_CAP_DIRECT_IO_ALLOW_MMAP)
    outargflags |= FUSE_DIRECT_IO_ALLOW_MMAP;
  if(f.conn.want & FUSE_CAP_HANDLE_KILLPRIV)
    outargflags |= FUSE_HANDLE_KILLPRIV;
  if(f.conn.want & FUSE_CAP_HANDLE_KILLPRIV_V2)
    outargflags |= FUSE_HANDLE_KILLPRIV_V2;
  if(f.conn.want & FUSE_CAP_ALLOW_IDMAP)
    outargflags |= FUSE_ALLOW_IDMAP;

  if(f.conn.want & FUSE_CAP_PASSTHROUGH)
    {
      outargflags |= FUSE_PASSTHROUGH;
      outarg.max_stack_depth = fuse_cfg.passthrough_max_stack_depth;
    }

  if((inargflags & FUSE_REQUEST_TIMEOUT) && fuse_cfg.request_timeout)
    {
      outargflags |= FUSE_REQUEST_TIMEOUT;
      outarg.request_timeout = fuse_cfg.request_timeout;
    }

  if(inargflags & FUSE_INIT_EXT)
    {
      outargflags |= FUSE_INIT_EXT;
      outarg.flags2 = (outargflags >> 32);
    }

  outarg.flags = outargflags;

  outarg.max_readahead = max_readahead;
  outarg.max_write = max_write;
  if(f.conn.proto_minor >= 13)
    {
      if(fuse_cfg.max_background >= (1 << 16))
        fuse_cfg.max_background = ((1 << 16) - 1);
      if(fuse_cfg.congestion_threshold > fuse_cfg.max_background)
        fuse_cfg.congestion_threshold = fuse_cfg.max_background;
      if(!fuse_cfg.congestion_threshold)
        fuse_cfg.congestion_threshold = (fuse_cfg.max_background * 3 / 4);

      outarg.max_background = fuse_cfg.max_background;
      outarg.congestion_threshold = fuse_cfg.congestion_threshold;
    }

  if(f.conn.proto_minor >= 23)
    outarg.time_gran = 1;

  size_t outargsize;
  if(arg->minor < 5)
    outargsize = FUSE_COMPAT_INIT_OUT_SIZE;
  else if(arg->minor < 23)
    outargsize = FUSE_COMPAT_22_INIT_OUT_SIZE;
  else
    outargsize = sizeof(outarg);

  fuse_syslog_fuse_init_out(&outarg);

  if(fuse_cfg.debug)
    fuse_debug_init_out(req->ctx.unique,&outarg,outargsize);

  send_reply_ok(req, &outarg, outargsize);
}

static
void
do_destroy(fuse_req_t            *req,
           struct fuse_in_header *hdr_)
{
  f.got_destroy = 1;

  f.op.destroy();

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
do_notify_reply(fuse_req_t            *req,
                struct fuse_in_header *hdr_)
{
  struct fuse_notify_req *nreq;
  struct fuse_notify_req *head;

  mutex_lock(f.lock);
  head = &f.notify_list;
  for(nreq = head->next; nreq != head; nreq = nreq->next)
    {
      if(nreq->unique == req->ctx.unique)
        {
          list_del_nreq(nreq);
          break;
        }
    }
  mutex_unlock(f.lock);

  if(nreq != head)
    nreq->reply(nreq, req, hdr_->nodeid, &hdr_[1]);
}

static
void
do_copy_file_range(fuse_req_t            *req_,
                   struct fuse_in_header *hdr_)
{
  f.op.copy_file_range(req_,hdr_);
}

static
void
do_copy_file_range_64(fuse_req_t            *req_,
                      struct fuse_in_header *hdr_)
{
  f.op.copy_file_range(req_,hdr_);
}

static
void
do_setupmapping(fuse_req_t            *req_,
                struct fuse_in_header *hdr_)
{
  f.op.setupmapping(req_,hdr_);
}

static
void
do_removemapping(fuse_req_t            *req_,
                 struct fuse_in_header *hdr_)
{
  f.op.removemapping(req_,hdr_);
}

static
void
do_syncfs(fuse_req_t            *req_,
          struct fuse_in_header *hdr_)
{
  f.op.syncfs(req_,hdr_);
}

static
void
do_tmpfile(fuse_req_t            *req_,
           struct fuse_in_header *hdr_)
{
  f.op.tmpfile(req_,hdr_);
}

static
void
do_statx(fuse_req_t            *req_,
         struct fuse_in_header *hdr_)
{
  f.op.statx(req_,hdr_);
}

static
void
do_rename2(fuse_req_t            *req_,
           struct fuse_in_header *hdr_)
{
  f.op.rename2(req_,hdr_);
}

static
void
do_lseek(fuse_req_t            *req_,
         struct fuse_in_header *hdr_)
{
  f.op.lseek(req_,hdr_);
}

static
int
send_notify_iov(struct fuse_session *se,
                int                  notify_code,
                struct iovec        *iov,
                int                  count)
{
  struct fuse_out_header out;

  if(!f.got_init)
    return -ENOTCONN;

  out.unique = 0;
  out.error = notify_code;
  iov[0].iov_base = &out;
  iov[0].iov_len = sizeof(struct fuse_out_header);

  return fuse_send_msg(se->fd, iov, count);
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

      return send_notify_iov(ph->se, FUSE_NOTIFY_POLL, iov, 2);
    }
  else
    {
      return 0;
    }
}

int
fuse_lowlevel_notify_inval_inode(struct fuse_session *se,
                                 uint64_t             ino,
                                 off_t                off,
                                 off_t                len)
{
  struct fuse_notify_inval_inode_out outarg;
  struct iovec iov[2];

  if(!se)
    return -EINVAL;

  outarg.ino = ino;
  outarg.off = off;
  outarg.len = len;

  iov[1].iov_base = &outarg;
  iov[1].iov_len  = sizeof(outarg);

  return send_notify_iov(se, FUSE_NOTIFY_INVAL_INODE, iov, 2);
}

int
fuse_lowlevel_notify_inval_entry(struct fuse_session *se,
                                 uint64_t             parent,
                                 const char          *name,
                                 size_t               namelen)
{
  struct fuse_notify_inval_entry_out outarg;
  struct iovec iov[3];

  if(!se)
    return -EINVAL;

  outarg.parent  = parent;
  outarg.namelen = namelen;
  // TODO: Add ability to set `flags`
  outarg.flags   = 0;

  iov[1].iov_base = &outarg;
  iov[1].iov_len = sizeof(outarg);
  iov[2].iov_base = (void *)name;
  iov[2].iov_len = namelen + 1;

  return send_notify_iov(se, FUSE_NOTIFY_INVAL_ENTRY, iov, 3);
}

int
fuse_lowlevel_notify_delete(struct fuse_session *se,
                            uint64_t             parent,
                            uint64_t             child,
                            const char          *name,
                            size_t               namelen)
{
  struct fuse_notify_delete_out outarg;
  struct iovec iov[3];

  if(!se)
    return -EINVAL;

  if(f.conn.proto_minor < 18)
    return -ENOSYS;

  outarg.parent = parent;
  outarg.child = child;
  outarg.namelen = namelen;
  outarg.padding = 0;

  iov[1].iov_base = &outarg;
  iov[1].iov_len = sizeof(outarg);
  iov[2].iov_base = (void *)name;
  iov[2].iov_len = namelen + 1;

  return send_notify_iov(se, FUSE_NOTIFY_DELETE, iov, 3);
}

struct fuse_retrieve_req
{
  struct fuse_notify_req nreq;
  void *cookie;
};

static
void
fuse_ll_retrieve_reply(struct fuse_notify_req *nreq,
                       fuse_req_t             *req,
                       uint64_t                ino,
                       const void             *inarg)
{
  struct fuse_retrieve_req *rreq =
    container_of(nreq, struct fuse_retrieve_req, nreq);

  fuse_reply_none(req);

  free(rreq);
}

int
fuse_lowlevel_notify_retrieve(struct fuse_session *se,
                                uint64_t             ino,
                                size_t               size,
                                off_t                offset,
                                void                *cookie)
{
  struct fuse_notify_retrieve_out outarg;
  struct iovec iov[2];
  struct fuse_retrieve_req *rreq;
  int err;

  if(!se)
    return -EINVAL;

  if(f.conn.proto_minor < 15)
    return -ENOSYS;

  rreq = (fuse_retrieve_req*)malloc(sizeof(*rreq));
  if(rreq == NULL)
    return -ENOMEM;

  mutex_lock(f.lock);
  rreq->cookie = cookie;
  rreq->nreq.unique = f.notify_ctr++;
  rreq->nreq.reply = fuse_ll_retrieve_reply;
  list_add_nreq(&rreq->nreq, &f.notify_list);
  mutex_unlock(f.lock);

  outarg.notify_unique = rreq->nreq.unique;
  outarg.nodeid = ino;
  outarg.offset = offset;
  outarg.size = size;

  iov[1].iov_base = &outarg;
  iov[1].iov_len = sizeof(outarg);

  err = send_notify_iov(se, FUSE_NOTIFY_RETRIEVE, iov, 2);
  if(err)
    {
      mutex_lock(f.lock);
      list_del_nreq(&rreq->nreq);
      mutex_unlock(f.lock);
      free(rreq);
    }

  return err;
}

#define FUSE_OPCODE_LEN (FUSE_COPY_FILE_RANGE_64 + 1)

typedef void (*fuse_ll_func)(fuse_req_t*, struct fuse_in_header *);
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
    NULL,
    NULL,
    NULL,
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
    do_statx,
    do_copy_file_range_64
  };

#define FUSE_MAXOPS (sizeof(fuse_ll_funcs) / sizeof(fuse_ll_funcs[0]))

static
void
fuse_ll_destroy(void *data)
{
  if(f.got_init && !f.got_destroy)
    f.op.destroy();

  mutex_destroy(f.lock);
  msgbuf_gc();
}

static
void
fuse_send_errno(const int            fd_,
                const int            errno_,
                const uint64_t       unique_id_)
{
  struct fuse_out_header out = {};
  struct iovec           iov = {};

  out.unique   = unique_id_;
  out.error    = -errno_;
  out.len      = sizeof(struct fuse_out_header);
  iov.iov_base = &out;
  iov.iov_len  = sizeof(struct fuse_out_header);

  if(fuse_cfg.debug)
    fuse_debug_out_header(&out);

  fuse_send_msg(fd_,&iov,1);
}

static
void
fuse_send_enomem(const int            fd_,
                 const uint64_t       unique_id_)
{
  fuse_send_errno(fd_,ENOMEM,unique_id_);
}

static
int
fuse_ll_buf_receive_read(struct fuse_session *se_,
                         int                  fd_,
                         fuse_msgbuf_t       *msgbuf_)
{
  int rv;

  rv = read(fd_,msgbuf_->mem,msgbuf_->size);
  if(rv == -1)
    return -errno;

  if(rv < (int)sizeof(struct fuse_in_header))
    {
      fprintf(stderr, "short read from fuse device\n");
      return -EIO;
    }

  return rv;
}

/*
 * read(2)-loop that reads exactly 'len_' bytes from 'fd_' into 'buf_'.
 * Returns 0 on success, -errno on error.
 */
static
int
fuse_ll_read_full(const int fd_,
                  char     *buf_,
                  const u32 len_)
{
  u32 done = 0;

  while(done < len_)
    {
      ssize_t rv = read(fd_,buf_ + done,len_ - done);
      if(rv == -1)
        {
          if(errno == EINTR)
            continue;
          return -errno;
        }
      if(rv == 0)
        return -EIO;

      done += rv;
    }

  return 0;
}


/* splice(2)/SPLICE_F_MOVE are Linux-only; this whole receive path
   (and its selection in fuse_lowlevel_new_common below) is compiled
   only on Linux. Other platforms always use the portable
   fuse_ll_buf_receive_read receive function. */
#if defined(__linux__)
static
int
fuse_ll_buf_receive_splice(struct fuse_session *se_,
                           int                  fd_,
                           fuse_msgbuf_t       *msgbuf_)
{
  if(!f.got_init)
    return fuse_ll_buf_receive_read(se_,fd_,msgbuf_);

  /* fuse_init may have disabled splice at INIT time (e.g. sysctl
     pipe-max-size too small). The receive fn was selected before INIT
     ran, so re-check here and take the plain read path cleanly
     rather than failing per-msgbuf into the fallback. */
  if(!fuse_cfg.splice_read && !fuse_cfg.splice_write)
    return fuse_ll_buf_receive_read(se_,fd_,msgbuf_);

  if(msgbuf_ensure_pipe(msgbuf_) != 0)
    {
      fuse_splice_fallback_log(splice_fallback_tag_t::RECEIVE_MSGBUF_PIPE);
      return fuse_ll_buf_receive_read(se_,fd_,msgbuf_);
    }

  /* One pipe-limited read = one whole FUSE request (pipe sized >= max
     request by the max_pages clamp in fuse_init). */
  ssize_t splice_n = splice(fd_,nullptr,msgbuf_->pipefd[1],nullptr,msgbuf_->size,SPLICE_F_MOVE);
  if(splice_n == -1)
    {
      int err = errno;
      if(err == EINTR)
        return -EINTR;
      /* splice(2) not usable on this combination; fall back to the
         read path for this message only. fuse_cfg.splice_read/write
         are deliberately left set - a single failure here (e.g. a
         transient ENOMEM) should not permanently disable splice for
         the whole mount; if the failure is systemic every subsequent
         message will simply retry and fall back the same way. */
      fuse_splice_fallback_log(splice_fallback_tag_t::RECEIVE_SPLICE_ERR);
      return fuse_ll_buf_receive_read(se_,fd_,msgbuf_);
    }

  if(splice_n < (int)sizeof(struct fuse_in_header))
    {
      fprintf(stderr,"fuse: short splice from fuse device: %zu < %zu\n",
              splice_n,sizeof(struct fuse_in_header));
      return -EIO;
    }

  /* Pull the fixed-size header back into memory. */
  ssize_t rv = fuse_ll_read_full(msgbuf_->pipefd[0],
                                msgbuf_->mem,
                                sizeof(struct fuse_in_header));
  if(rv != 0)
    return (int)rv;

  struct fuse_in_header *in = (struct fuse_in_header*)msgbuf_->mem;

  /* Header sanity: in->len <= what splice actually moved. */
  if(in->len > (u32)splice_n)
    {
      fprintf(stderr,"fuse: splice header size %u exceeds move %zu\n",
              in->len,splice_n);
      return -EIO;
    }

  /* Header sanity: in->len must be at least large enough to hold the
     fixed fuse_in_header, or the subtraction below underflows (u32)
     into a huge value that would then be used as a read length -
     draining whatever's left in the pipe and blocking the worker
     thread forever on the now-empty pipe (the write end stays open,
     so read() blocks rather than returning EOF). A well-behaved
     kernel never sends this; guard anyway since this path (unlike the
     plain read receive) does arithmetic on in->len before the generic
     opcode dispatch ever sees it. */
  if(in->len < sizeof(struct fuse_in_header))
    {
      fprintf(stderr,"fuse: splice header size %u smaller than fuse_in_header\n",
              in->len);
      return -EIO;
    }

  u32 hdrlen = in->len - sizeof(struct fuse_in_header);

  if(in->opcode == FUSE_WRITE && fuse_cfg.splice_write)
    {
      /* Also copy the write_in header out of the pipe so fuse_lib_write
         can parse it without touching the pipe. */
      u32 write_hdr = sizeof(struct fuse_write_in);

      /* Same underflow guard as above, sized for this opcode: a
         WRITE request must be at least header + fuse_write_in, or
         both this read (fixed write_hdr length, could ask for more
         than the pipe will ever hold) and the payload computation
         below (which would underflow) are wrong. */
      if(hdrlen < write_hdr)
        {
          fprintf(stderr,"fuse: splice WRITE request too short for"
                  " fuse_write_in: %u < %u\n",
                  hdrlen,write_hdr);
          return -EIO;
        }

      int wirv = fuse_ll_read_full(msgbuf_->pipefd[0],
                                  msgbuf_->mem + sizeof(struct fuse_in_header),
                                  write_hdr);
      if(wirv != 0)
        return wirv;

      /* Payload stays in the pipe for write_buf zero-copy. */
      u32 payload = in->len - sizeof(struct fuse_in_header) - write_hdr;
      msgbuf_->pipe_used = payload;
      return (int)splice_n;
    }

  /* Non-write path: move the rest of the request out of the pipe and
     into the msgbuf so downstream processing is identical to read(). */
  rv = fuse_ll_read_full(msgbuf_->pipefd[0],
                        msgbuf_->mem + sizeof(struct fuse_in_header),
                        hdrlen);
  if(rv != 0)
    return (int)rv;

  msgbuf_->pipe_used = 0;

  return in->len;
}
#endif /* defined(__linux__) */

static
void
fuse_ll_buf_process_read(struct fuse_session *se_,
                         int                  fd_,
                         const fuse_msgbuf_t *msgbuf_)
{
  int err;
  struct fuse_req_t *req;
  struct fuse_in_header *in;

  in = (struct fuse_in_header*)msgbuf_->mem;

  if(fuse_cfg.debug)
    fuse_debug_in_header(in);

  req = fuse_req_alloc();
  if(req == NULL)
    return fuse_send_enomem(fd_,in->unique);

  req->ctx.len    = in->len;
  req->ctx.opcode = in->opcode;
  req->ctx.unique = in->unique;
  req->ctx.nodeid = in->nodeid;
  req->ctx.uid    = in->uid;
  req->ctx.gid    = in->gid;
  req->ctx.pid    = in->pid;
  req->ctx.umask  = 0;
  req->conn       = f.conn;
  req->se         = se_;
  req->fd         = fd_;
  req->ioctl_64bit = 0;
  req->msgbuf     = (fuse_msgbuf_t*)msgbuf_;

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
                              int                  fd_,
                              const fuse_msgbuf_t *msgbuf_)
{
  int err;
  fuse_req_t *req;
  struct fuse_in_header *in;

  in = (struct fuse_in_header*)msgbuf_->mem;

  if(fuse_cfg.debug)
    fuse_debug_in_header(in);

  req = fuse_req_alloc();
  if(req == NULL)
    return fuse_send_enomem(fd_,in->unique);

  req->ctx.len    = in->len;
  req->ctx.opcode = in->opcode;
  req->ctx.unique = in->unique;
  req->ctx.nodeid = in->nodeid;
  req->ioctl_64bit = 0;
  req->ctx.uid    = in->uid;
  req->ctx.gid    = in->gid;
  req->ctx.pid    = in->pid;
  req->ctx.umask  = 0;
  req->se         = se_;
  req->fd         = fd_;
  req->msgbuf     = (fuse_msgbuf_t*)msgbuf_;

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
                         size_t                          op_size)
{
  struct fuse_session *se;

  if(sizeof(struct fuse_lowlevel_ops) < op_size)
    {
      fprintf(stderr, "fuse: warning: library too old, some operations may not work\n");
      op_size = sizeof(struct fuse_lowlevel_ops);
    }

  list_init_nreq(&f.notify_list);
  f.notify_ctr = 1;
  mutex_init(f.lock);

  memcpy(&f.op,op,op_size);
  f.owner = getuid();

  {
    /* Splice-capable receive needs a pipe buffer per msgbuf.
       fuse_ll_buf_receive_splice only exists on Linux (see its
       definition above); other platforms always take the portable
       read() receive path. */
#if defined(__linux__)
    if(fuse_cfg.splice_read || fuse_cfg.splice_write)
      se = fuse_session_new(&f,
                            (void*)fuse_ll_buf_receive_splice,
                            (void*)fuse_ll_buf_process_read_init,
                            (void*)fuse_ll_destroy);
    else
#endif
      se = fuse_session_new(&f,
                            (void*)fuse_ll_buf_receive_read,
                            (void*)fuse_ll_buf_process_read_init,
                            (void*)fuse_ll_destroy);
  }

  if(!se)
    goto out_free;

  return se;

 out_free:
  mutex_destroy(f.lock);

  return nullptr;
}

struct fuse_session*
fuse_lowlevel_new(struct fuse_args               *args,
                  const struct fuse_lowlevel_ops *op,
                  size_t                          op_size)
{
  return fuse_lowlevel_new_common(args, op, op_size);
}
