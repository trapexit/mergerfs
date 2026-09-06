/*
  ISC License

  Copyright (c) 2022, Antonio SJ Musumeci <trapexit@spawn.link>
  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <atomic>


#include "fuse_msgbuf.hpp"

#include "fatal.hpp"
#include "fuse.h"
#include "fuse_kernel.h"
#include "syslog.hpp"

/* Warn-once-per-process-per-tag logger shared with fuse_lowlevel.cpp
   (declared in fuse_msgbuf.hpp) so the two translation units don't
   each hand-roll their own copy of this mechanism. Each tag belongs
   to one of two "domains" - the msgbuf/receive-side pipe plumbing in
   this file, or the reply/splice-move plumbing in fuse_lowlevel.cpp -
   which only differ in the fixed prefix/description text; the tag
   name itself (embedded in both lines) still identifies the exact
   call site. */
struct SpliceFallbackInfo
{
  const char *name;
  bool        msgbuf_domain;
};

static constexpr SpliceFallbackInfo SPLICE_FALLBACK_INFO[] =
  {
    { "pipe-max-read-fallback",   true  },
    { "msgbuf-fsetpipe-ep",       true  },
    { "msgbuf-fsetpipe-any",      true  },
    { "msgbuf-pipe-create",       true  },
    { "reply-pipe-fsetpipe-ep",   false },
    { "reply-pipe-fsetpipe-any",  false },
    { "reply-pipe-ensure",        false },
    { "reply-pipe-cap",           false },
    { "reply-vmsplice-hdr",       false },
    { "reply-fd-splice",          false },
    { "receive-msgbuf-pipe",      false },
    { "receive-splice-err",       false },
    { "reply-splice-move-writev", false },
  };

void
fuse_splice_fallback_log(splice_fallback_tag_t tag_)
{
  static std::atomic<bool> seen[sizeof(SPLICE_FALLBACK_INFO) /
                                sizeof(SPLICE_FALLBACK_INFO[0])];
  const SpliceFallbackInfo &info = SPLICE_FALLBACK_INFO[(size_t)tag_];
  bool expected = false;

  if(!seen[(size_t)tag_].compare_exchange_strong(expected,true))
    return;

  const char *prefix      = (info.msgbuf_domain ?
                             "msgbuf pipe fallback" : "splice fallback");
  const char *description = (info.msgbuf_domain ?
                             "splice receive will use a smaller/default pipe" :
                             "splice I/O will use the copy path for this mount");

  SysLog::warning("{} [{}]: {}",prefix,info.name,description);
  /* Explicit stderr echo (rather than syslog's LOG_PERROR, which
     would apply to every log message in the process): keeps this
     one class of message visible on a foreground (-f) run without
     doubling unrelated log output under systemd/journald. */
  fprintf(stderr,"mergerfs: %s [%s]: %s\n",prefix,info.name,description);
}

#include "mutex.hpp"
#include "objpool.hpp"

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <charconv>
#include <climits>
#include <cstdio>
#include <cstdlib>


static u32 g_pagesize = 0;
static u64 g_bufsize  = 0;

/* Extra pages: 1 for page-alignment headroom, 1 for
   fuse_in_header+fuse_write_in alignment */
static constexpr u64 MSGBUF_OVERHEAD_PAGES = 2ULL;

/* Guards g_cached_pipes (defined below, near its struct types.) Declared
   here (ahead of _constructor(), which initializes it) rather than
   next to g_cached_pipes itself. */
static mutex_t g_cached_pipes_mutex;

static
__attribute__((constructor))
void
_constructor()
{
  long pagesize = sysconf(_SC_PAGESIZE);

  if(pagesize <= 0)
    fatal::abort("pagesize query failed - {}",strerror(errno));

  g_pagesize = pagesize;

  assert((sizeof(struct fuse_in_header) + sizeof(struct fuse_write_in))
         < g_pagesize);

  msgbuf_set_bufsize(FUSE_DEFAULT_MAX_MAX_PAGES);

  mutex_init(g_cached_pipes_mutex);
}

struct PageAlignedAllocator
{
  void*
  allocate(size_t size_, size_t align_)
  {
    void *buf = nullptr;
    int rv = posix_memalign(&buf, align_, size_);
    return (rv == 0) ? buf : nullptr;
  }

  void
  deallocate(void *ptr_, size_t /*size_*/, size_t /*align_*/)
  {
    free(ptr_);
  }
};

struct ShouldPoolMsgbuf
{
  bool operator()(const fuse_msgbuf_t *msgbuf) const noexcept
  {
    return msgbuf->size == (g_bufsize - g_pagesize);
  }
};

static ObjPool<fuse_msgbuf_t, PageAlignedAllocator, ShouldPoolMsgbuf> g_msgbuf_pool;

static
void
_msgbuf_pipe_init(fuse_msgbuf_t *msgbuf_)
{
  msgbuf_->pipefd[0] = -1;
  msgbuf_->pipefd[1] = -1;
  msgbuf_->pipe_used = 0;
  msgbuf_->pipe_cap  = 0;
}

static
void
_msgbuf_page_align(fuse_msgbuf_t *msgbuf_)
{
  msgbuf_->mem   = (char*)msgbuf_;
  msgbuf_->mem  += g_pagesize;
  msgbuf_->size  = (g_bufsize - g_pagesize);

  _msgbuf_pipe_init(msgbuf_);
}

static
void
_msgbuf_write_align(fuse_msgbuf_t *msgbuf_)
{
  msgbuf_->mem   = (char*)msgbuf_;
  msgbuf_->mem  += g_pagesize;
  msgbuf_->mem  -= sizeof(struct fuse_in_header);
  msgbuf_->mem  -= sizeof(struct fuse_write_in);
  msgbuf_->size  = (g_bufsize - g_pagesize);

  _msgbuf_pipe_init(msgbuf_);
}

/*
 * The ObjPool value-initializes msgbufs on every alloc, which would
 * clobber live pipe fds carried by a pooled buffer, so pipes are not
 * stored across free/alloc cycles. Instead released pipes are stashed
 * in a small process-global cache keyed by capacity so the next
 * msgbuf_ensure_pipe() usually avoids pipe2(2) and
 * fcntl(F_SETPIPE_SZ) entirely.
 *
 * Global rather than thread-local: with the opt-in process-thread
 * pool, msgbuf_free() runs on worker threads while msgbuf_ensure_pipe()
 * runs on read threads - a thread-local cache can never hand a pipe
 * across that boundary, and every request would pay pipe2(2) +
 * F_SETPIPE_SZ + close. The cache is guarded by g_cached_pipes_mutex
 * (mutex_lockguard, same idiom as f.lock in fuse.cpp) - the ops it
 * protects are O(1) and vastly cheaper than the syscalls they avoid.
 */
struct cached_pipe_t
{
  int  fd0{-1};
  int  fd1{-1};
  u32  cap{0};
};

struct cached_pipes_t
{
  static constexpr u32 MAX = 16;

  u32           count{0};
  cached_pipe_t entries[MAX];
};

static cached_pipes_t g_cached_pipes;

static
u32
_read_pipe_max_size(void)
{
  int fd = ::open("/proc/sys/fs/pipe-max-size",O_RDONLY | O_CLOEXEC);
  if(fd == -1)
    {
      fuse_splice_fallback_log(splice_fallback_tag_t::PIPE_MAX_READ_FALLBACK);
      return 1048576;
    }

  char    buf[32];
  ssize_t rv = ::read(fd,buf,sizeof(buf));
  ::close(fd);
  if(rv <= 0)
    {
      fuse_splice_fallback_log(splice_fallback_tag_t::PIPE_MAX_READ_FALLBACK);
      return 1048576;
    }

  unsigned long v;
  auto [end,ec] = std::from_chars(buf,buf + rv,v);
  if(ec != std::errc() || v == 0)
    {
      fuse_splice_fallback_log(splice_fallback_tag_t::PIPE_MAX_READ_FALLBACK);
      return 1048576;
    }

  /* Clamp rather than treat as unreadable: a legitimately huge
     sysctl (e.g. an admin-set multi-GiB pipe-max-size) must not
     be mistaken for "couldn't read it" and silently replaced by
     the small 1MiB default below. */
  return (v > UINT32_MAX) ? UINT32_MAX : (u32)v;
}

u32
fuse_effective_pipe_max()
{
  static const u32 max = _read_pipe_max_size();

  return max;
}

/* Shared by msgbuf_ensure_pipe (this file) and _reply_pipe_ensure
   (fuse_lowlevel.cpp): try F_SETPIPE_SZ at cap_, retrying once at the
   1MiB default on failure. tag_any_ is logged whenever the sized
   request fails at all (even if the retry then succeeds); tag_ep_ is
   logged only if every attempt fails. */
int
fuse_pipe_set_size(int                   fd_,
                   u32                   cap_,
                   bool                  retry_unconditionally_,
                   splice_fallback_tag_t tag_any_,
                   splice_fallback_tag_t tag_ep_)
{
  int err = 0;

  /* F_SETPIPE_SZ takes a signed int; a cap of 2^31 or higher (reachable
     when pipe-max-size and the negotiated fuse-msg-size both pass
     ~2GiB, e.g. large-page hosts) would sign-extend into a negative
     fcntl() argument below. Clamp to INT_MAX rather than cast blindly. */
  if(cap_ > (u32)INT_MAX)
    cap_ = (u32)INT_MAX;

  int rv = ::fcntl(fd_,F_SETPIPE_SZ,(int)cap_);
  if(rv == -1)
    {
      /* Capture immediately: fuse_splice_fallback_log() below can
         itself clobber errno before it's read again further down. */
      err = -errno;
      fuse_splice_fallback_log(tag_any_);
      if(retry_unconditionally_ || err == -EPERM)
        {
          rv = ::fcntl(fd_,F_SETPIPE_SZ,1048576);
          if(rv == -1)
            err = -errno; /* re-capture: this is a different syscall */
        }
      if(rv <= 0)
        fuse_splice_fallback_log(tag_ep_);
    }

  return (rv > 0) ? rv : err;
}

/* Shared failure-cleanup for msgbuf_ensure_pipe: close the fds when a
   pipe was actually created (fd0_ != -1), reset the msgbuf's pipe
   state, and return rv_ (the errno-or-fixed-code the caller wants to
   propagate). */
static
int
_msgbuf_ensure_pipe_fail(fuse_msgbuf_t *msgbuf_,
                         int            fd0_,
                         int            fd1_,
                         int            rv_)
{
  if(fd0_ != -1)
    {
      ::close(fd0_);
      ::close(fd1_);
    }
  msgbuf_->pipefd[0] = -1;
  msgbuf_->pipefd[1] = -1;
  msgbuf_->pipe_cap  = 0;

  return rv_;
}

int
msgbuf_ensure_pipe(fuse_msgbuf_t *msgbuf_)
{
#if !defined(__linux__)
  /* F_SETPIPE_SZ (used below to size the pipe to fit a whole splice
     receive) is Linux-only. Without it a splice receive can't be
     reliably sized, so fail unconditionally here and let every
     caller fall back to the portable read() receive path, exactly
     as documented ("splice remains unavailable on platforms that
     lack it; the copy path is the fallback everywhere"). */
  (void)msgbuf_;
  return -ENOTSUP;
#else
  u32 cap = (u32)std::min((u64)g_bufsize,(u64)fuse_effective_pipe_max());

  /* F_SETPIPE_SZ takes a signed int; a cap of 2^31 or higher (reachable
     when pipe-max-size and the negotiated fuse-msg-size both pass
     ~2GiB, e.g. large-page hosts) would sign-extend into a negative
     fcntl() argument below. Clamp to INT_MAX rather than cast blindly -
     mirrors the identical clamp inside fuse_pipe_set_size, but the
     cache lookup just below also needs the clamped value. */
  if(cap > (u32)INT_MAX)
    cap = (u32)INT_MAX;

  if(msgbuf_->pipefd[0] != -1)
    return 0;

  /* try global cache first */
  {
    mutex_lockguard(g_cached_pipes_mutex);
    for(u32 i = 0; i < g_cached_pipes.count; i++)
      {
        cached_pipe_t *cp = &g_cached_pipes.entries[i];
        if(cp->cap >= cap)
          {
            msgbuf_->pipefd[0] = cp->fd0;
            msgbuf_->pipefd[1] = cp->fd1;
            msgbuf_->pipe_cap  = cp->cap;
            g_cached_pipes.entries[i] = g_cached_pipes.entries[g_cached_pipes.count - 1];
            g_cached_pipes.count--;
            /* mutex_lockguard's unlock is deferred to scope exit, which
               this return still triggers (stack unwinding runs it
               regardless of which nested block the return sits in). */
            return 0;
          }
      }
  }

  int rv = ::pipe2(msgbuf_->pipefd,O_CLOEXEC);
  if(rv == -1)
    {
      int err = -errno; /* capture before the log call: SysLog::warning
                            -> syslog()/LOG_PERROR can make its own
                            syscalls (connect/sendto/write) and clobber
                            errno first. */
      fuse_splice_fallback_log(splice_fallback_tag_t::MSGBUF_PIPE_CREATE);
      msgbuf_->pipefd[0] = -1;
      msgbuf_->pipefd[1] = -1;
      return err;
    }

  /* Only retry unconditionally on EPERM: unlike the reply pipe
     (fuse_lowlevel.cpp), a non-EPERM failure here (e.g. EINVAL on
     an already-too-large ask) should not mask itself with a 1MiB
     retry that might still be wrong for this request. */
  rv = fuse_pipe_set_size(msgbuf_->pipefd[0],cap,/*retry_unconditionally_=*/false,
                          splice_fallback_tag_t::MSGBUF_FSETPIPE_ANY,
                          splice_fallback_tag_t::MSGBUF_FSETPIPE_EP);
  if(rv <= 0)
    return _msgbuf_ensure_pipe_fail(msgbuf_,msgbuf_->pipefd[0],msgbuf_->pipefd[1],rv);

  /* Record what F_SETPIPE_SZ actually granted, not what we asked
     for: after an EPERM->1MiB retry the real capacity can be
     smaller than the ask, and the process-global pipe cache must
     not serve this pipe for a request it cannot hold (a short
     receive splice would then fail the in->len check and kill the
     worker thread). */
  msgbuf_->pipe_cap = (u32)rv;

  /* The EPERM->1MiB retry (or a cache hit sized for a smaller
     g_bufsize than this msgbuf now needs) can grant a pipe smaller
     than "cap" - what THIS request actually requires to hold one
     whole message. Handing that pipe back as success would let the
     caller splice from /dev/fuse into it, come up short
     (splice_n < in->len), and return -EIO, which fuse_loop.cpp's
     _retriable_receive_error does not treat as retriable: that
     tears down the entire session over one thread's transient
     pipe-sizing shortfall. Fail HERE instead, before any /dev/fuse
     data has been touched, so the caller falls back to the plain
     read() receive for this message like every other
     msgbuf_ensure_pipe failure already does. */
  if(msgbuf_->pipe_cap < cap)
    {
      fuse_splice_fallback_log(splice_fallback_tag_t::MSGBUF_FSETPIPE_EP);
      return _msgbuf_ensure_pipe_fail(msgbuf_,msgbuf_->pipefd[0],msgbuf_->pipefd[1],-ENOSPC);
    }

  return 0;
#endif /* defined(__linux__) */
}

void
fuse_drain_pipe_fd(int    fd_,
                   void  *scratch_,
                   size_t scratch_size_)
{
  if(fd_ == -1)
    return;

  for(;;)
    {
      int avail;

      if(::ioctl(fd_,FIONREAD,&avail) == -1)
        break;
      if(avail <= 0)
        break;

      ssize_t rv = ::read(fd_,scratch_,std::min((size_t)avail,scratch_size_));
      if(rv == -1)
        {
          if(errno == EINTR)
            continue;
          break;
        }
      if(rv == 0)
        break;
    }
}

void
msgbuf_drain_pipe(fuse_msgbuf_t *msgbuf_)
{
  /* This is the cleanup path for failed spliced writes (ENOSPC etc.),
     where the payload may have been fully or partially consumed
     already. */
  fuse_drain_pipe_fd(msgbuf_->pipefd[0],msgbuf_->mem,msgbuf_->size);

  msgbuf_->pipe_used = 0;
}

static
void
_msgbuf_release_pipe(fuse_msgbuf_t *msgbuf_)
{
  if(msgbuf_->pipefd[0] == -1)
    return;

  /* Cache bookkeeping happens under lock; the close() below (taken only
     on cache overflow) deliberately happens AFTER the lock is released,
     so the mutex is never held across a syscall. */
  bool cached = false;
  {
    mutex_lockguard(g_cached_pipes_mutex);

    if(g_cached_pipes.count < cached_pipes_t::MAX)
      {
        cached_pipe_t *cp = &g_cached_pipes.entries[g_cached_pipes.count++];

        cp->fd0 = msgbuf_->pipefd[0];
        cp->fd1 = msgbuf_->pipefd[1];
        /* cache the capacity F_SETPIPE_SZ actually granted: every
           success path through msgbuf_ensure_pipe (cache hit or fresh
           fcntl) sets pipe_cap nonzero before pipefd[0] goes valid, so
           there is no "no fcntl ever ran" case to fall back for here. */
        cp->cap = msgbuf_->pipe_cap;
        cached  = true;
      }
  }

  if(!cached)
    {
      ::close(msgbuf_->pipefd[0]);
      ::close(msgbuf_->pipefd[1]);
    }

  msgbuf_->pipefd[0] = -1;
  msgbuf_->pipefd[1] = -1;
}

/* Close and drop every pipe currently sitting in the process-global
   cache. Used by msgbuf_clear() so the "gc" control-file command (the
   admin-facing "shrink resource usage now" entry point) actually
   reclaims the cache's fds/kernel pipe buffers instead of only
   clearing the msgbuf pool - previously the cache was documented as
   "reclaimed ... at process exit", but no such reclamation code
   existed anywhere, so in practice it was only ever trimmed by the
   count>=MAX overflow-close path inside _msgbuf_release_pipe. */
static
void
_cached_pipes_clear(void)
{
  cached_pipes_t local;

  /* Snapshot-and-clear under lock, close() (a syscall) after releasing
     it - same discipline as _msgbuf_release_pipe above. */
  {
    mutex_lockguard(g_cached_pipes_mutex);

    local = g_cached_pipes;
    g_cached_pipes.count = 0;
  }

  for(u32 i = 0; i < local.count; i++)
    {
      ::close(local.entries[i].fd0);
      ::close(local.entries[i].fd1);
    }
}

typedef void (*msgbuf_setup_func_t)(fuse_msgbuf_t*);

static
fuse_msgbuf_t*
_msgbuf_alloc(msgbuf_setup_func_t setup_func_)
{
  fuse_msgbuf_t *msgbuf;

  msgbuf = g_msgbuf_pool.alloc_size(g_bufsize);
  if(msgbuf == NULL)
    return NULL;

  setup_func_(msgbuf);

  return msgbuf;
}

fuse_msgbuf_t*
msgbuf_alloc()
{
  return _msgbuf_alloc(_msgbuf_write_align);
}

fuse_msgbuf_t*
msgbuf_alloc_page_aligned()
{
  return _msgbuf_alloc(_msgbuf_page_align);
}

void
msgbuf_free(fuse_msgbuf_t *msgbuf_)
{
  if(msgbuf_ == nullptr)
    return;

  /* Drain whenever a pipe exists, not only when pipe_used says bytes
     remain: receive-path error returns (short splice, header read
     failure, in->len sanity) leave queued bytes with pipe_used == 0,
     and releasing a dirty pipe into the global cache would corrupt
     the next request staged into it. msgbuf_drain_pipe is a
     FIONREAD-bounded no-op on a clean pipe. */
  if(msgbuf_->pipefd[0] != -1)
    msgbuf_drain_pipe(msgbuf_);

  _msgbuf_release_pipe(msgbuf_);

  g_msgbuf_pool.free_size(msgbuf_,g_bufsize);
}

u64
msgbuf_get_bufsize()
{
  return g_bufsize;
}

u32
msgbuf_get_pagesize()
{
  return g_pagesize;
}

void
msgbuf_set_bufsize(const u64 size_in_pages_)
{
  u64 new_bufsize;

  new_bufsize = ((size_in_pages_ + MSGBUF_OVERHEAD_PAGES) * g_pagesize);
  if(new_bufsize != g_bufsize)
    {
      g_bufsize = new_bufsize;
      /* Pool nodes are keyed by alloc size; a stale-size node must
         never be reused for the new bufsize, so drop the whole pool.
         (Pooled msgbufs never hold pipes: msgbuf_free always runs
         _msgbuf_release_pipe first, moving any pipe into the
         process-global cache or closing it. Cached pipes are keyed
         by granted capacity and remain usable across a bufsize
         change - msgbuf_ensure_pipe's first-fit only accepts
         cp->cap >= cap.) */
      g_msgbuf_pool.clear();
    }
}

u64
msgbuf_alloc_count()
{
  return g_msgbuf_pool.size();
}

void
msgbuf_gc()
{
  /* gc() deallocates objects inside ObjPool; any pipe a msgbuf held
     was already released to the process-global cache (or closed) by
     msgbuf_free's _msgbuf_release_pipe before the node returned to
     the pool. Nothing to reclaim here beyond delegating. The
     process-global cache itself (up to 16 pipes / 32 fds) is left
     alone by this lighter-weight periodic gc() - it survives across
     threads by design - but IS reclaimed by _cached_pipes_clear() via
     the heavier, explicitly-invoked msgbuf_clear(). */
  g_msgbuf_pool.gc();
}

void
msgbuf_clear()
{
  g_msgbuf_pool.clear();
  _cached_pipes_clear();
}
