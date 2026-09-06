/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for
  any purpose with or without fee is hereby granted, provided that the
  above copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
  WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
  AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
  DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
  PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
  PERFORMANCE OF THIS SOFTWARE.
*/

#include "fuse_write.hpp"
#include "fuse_write_buf.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_getfl.hpp"
#include "ioprio.hpp"
#include "state.hpp"

#include "fuse.h"

#include <cstring>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sys/ioctl.h>


/* Pure splice copy: pipe -> branch fd. Safe to run under fi->mutex's
   shared lock. Returns fuse_buf_copy's result: >= 0 bytes moved (the
   srcv_ idx/off mark what was consumed), or -errno with NOTHING moved
   (the source pipe is left intact). Never delegates and never locks
   beyond the caller's shared lock. */
static
int
_write_buf_splice(const int       fd_,
                  fuse_bufvec    *srcv_,
                  const off_t    offset_,
                  const size_t   size_)
{
  fuse_bufvec dst = fuse_bufvec_fd(fd_,offset_,size_,
                                  (fuse_buf_flags)(FUSE_BUF_IS_FD|
                                                   FUSE_BUF_FD_SEEK));
  const fuse_buf_copy_flags cpflags =
    (fuse_buf_copy_flags)(FUSE_BUF_SPLICE_MOVE | FUSE_BUF_SPLICE_NONBLOCK);

  ssize_t copied = fuse_buf_copy(&dst,srcv_,cpflags);

  return (int)copied;
}

/* Mem fallback for whatever the splice could not move (partial splice,
   O_DIRECT branch fd rejecting splice, or a spliced prefix followed by
   an error): read the remaining pipe payload into a heap buffer and
   delegate to FUSE::write - lock-free by design, because FUSE::write
   takes fi->mutex itself and owns the full move-on-enospc handling
   (re-check, move, pwriten retry). Any duplication of that logic here
   would re-enter the same non-recursive mutex and self-deadlock. */
static
int
_write_buf_fallback(const fuse_req_ctx_t   *ctx_,
                    const fuse_file_info_t *ffi_,
                    fuse_bufvec            *srcv_,
                    const off_t             offset_)
{
  size_t remaining = ((size_t)srcv_->buf[srcv_->idx].size - srcv_->off);
  if(remaining == 0)
    return 0;

  /* A failed splice attempt may have already consumed pipe bytes
     without advancing srcv_: on EINVAL (e.g. O_APPEND branch fds
     reject splice with an explicit offset) fuse_buf_splice falls
     back to fuse_buf_fd_to_fd, which reads from the pipe into a
     stack buffer and can fail (or return short) after that read,
     dropping the bytes it consumed. Reading `remaining` would then
     block forever on a pipe whose write end this request itself
     holds. FIONREAD bounds the read; a short pipe means payload
     bytes are gone, so fail the request - the app retries, and
     msgbuf_free's drain cleans the pipe. */
  int avail;
  if(ioctl(srcv_->buf[srcv_->idx].fd,FIONREAD,&avail) == -1)
    return -errno;
  /* avail is `int`; guard the sign before the size_t comparison below
     widens a negative value into a huge one and defeats this exact
     short-pipe check (avail should never be negative in practice, but
     the cast must not silently trust that). */
  if((avail < 0) || ((size_t)avail < remaining))
    return -EIO;

  std::unique_ptr<char[]> buf(new(std::nothrow) char[remaining]);
  if(!buf)
    return -ENOMEM;

  size_t to_read = remaining;
  while(to_read > 0)
    {
      ssize_t nr;

      nr = ::read(srcv_->buf[srcv_->idx].fd,buf.get() + (remaining - to_read),to_read);
      if(nr == -1)
        {
          if(errno == EINTR)
            continue;
          return -errno;
        }
      if(nr == 0)
        return -EIO;
      to_read -= nr;
    }

  return FUSE::write(ctx_,ffi_,buf.get(),remaining,offset_);
}

int
FUSE::write_buf(const fuse_req_ctx_t   *ctx_,
                const fuse_file_info_t *ffi_,
                struct fuse_bufvec     *srcv_,
                off_t                   offset_)
{
  ioprio::SetFrom iop(ctx_->pid);

  FileInfo *fi = state.get_fi(ctx_,ffi_->fh);
  if(fi == nullptr)
    return -EBADF;

  /* O_APPEND branch fds deterministically reject splice(2) with
     EINVAL (splice requires an explicit, seekable destination offset;
     O_APPEND ignores any offset and always writes at EOF instead), so
     _write_buf_splice would immediately fall through fuse_buf_splice
     into fuse_buf_fd_to_fd's byte-at-a-time bounce copy for the WHOLE
     payload. That bounce loop reads a chunk from the (unrereadable,
     one-shot) pipe before writing it to the branch fd; if the write
     fails partway (e.g. ENOSPC on a full branch after earlier chunks
     already landed), the just-read chunk is silently lost with no way
     to recover it, and _write_buf_fallback below can only refuse the
     request outright (its FIONREAD check trips) rather than let
     FUSE::write relocate it to another branch, because the payload it
     would need to resend is already gone. Skip the splice attempt
     entirely for O_APPEND: go straight to the fallback here, while
     the full unread payload is still intact in the pipe, so the
     lossless heap-buffer path (and FUSE::write's moveonenospc retry)
     is used from the start instead of being reached only after damage
     is done. cache.writeback=true strips O_APPEND from the branch
     fd itself (fuse_open.cpp: _tweak_flags_cache_writeback, gated
     on cfg.cache_writeback - an option independent of cache.files),
     so this only fires when cache.writeback is left at its default
     of false, regardless of the cache.files setting.

     O_DIRECT branch fds (ffi_->flags forwards the client's O_DIRECT
     open verbatim - see fuse_read_buf.cpp) can likewise reject
     splice(2) with EINVAL. Unlike O_APPEND that alone would just
     route the whole payload through the same lossless
     fuse_buf_fd_to_fd bounce (no data-loss hazard, since the offset
     is still explicit/seekable) - but a splice that partially
     succeeds before an O_DIRECT-related short count would leave
     _write_buf_fallback resuming at a non-block-aligned offset/size,
     which O_DIRECT destinations can reject even though the original,
     client-aligned request would have succeeded. Bypass splice
     entirely for O_DIRECT too so the fallback always sees the whole,
     originally-aligned request. */
  if(ffi_->flags & (O_APPEND|O_DIRECT))
    return _write_buf_fallback(ctx_,ffi_,srcv_,offset_);

  /* The check above reads fuse_write_in.flags, which the kernel only
     fills in for writes issued from a kiocb. Writeback flushes go
     through fuse_writepage_args_setup -> fuse_write_args_fill, which
     never assigns it, so a FUSE_WRITE_CACHE request (ffi_->writepage)
     always arrives with flags == 0 and slips past the guard into the
     lossy bounce described above. The branch fd really can be
     O_APPEND/O_DIRECT there: with cache.writeback=false via a
     MAP_SHARED mmap of a file opened O_APPEND (the O_APPEND strip in
     fuse_open.cpp is gated on cfg.cache_writeback), and with
     cache.writeback=true via an O_DIRECT handle, since writepages
     picks an arbitrary write-capable fh rather than the one that
     dirtied the pages. Ask the fd itself instead of trusting the
     request, and treat an unreadable answer as the worst case - the
     fallback is always correct, only slower. */
  if(ffi_->writepage)
    {
      int fl = fs::getfl(fi->fd);

      if((fl < 0) || (fl & (O_APPEND|O_DIRECT)))
        return _write_buf_fallback(ctx_,ffi_,srcv_,offset_);
    }

  /* Splice under the shared lock only - it makes no nested calls.
     Mirrors _write's discipline (fuse_write.cpp) so parallel direct
     writes serialize against mem-path writers on the same file. */
  size_t  size = fuse_buf_size(srcv_);
  ssize_t copied;
  {
    std::shared_lock<std::shared_mutex> slk(fi->mutex);

    copied = _write_buf_splice(fi->fd,srcv_,offset_,size);
  }

  /* copied < 0: nothing moved, pipe intact (fuse_buf_copy only
     returns errors when zero bytes were consumed).
     srcv_->idx < srcv_->count: partial move, remainder still queued.
     Either way the fallback drains the pipe and delegates to the
     mem write path lock-free; FUSE::write owns ENOSPC/move/retry. */
  if(copied < 0 || srcv_->idx < srcv_->count)
    {
      off_t done = (copied > 0) ? (off_t)copied : (off_t)0;

      int rv = _write_buf_fallback(ctx_,ffi_,srcv_,offset_ + done);
      if(rv < 0)
        {
          /* `done` bytes are already durably written to the branch
             fd via the splice above; a request-level error here must
             not discard that progress. Per this codebase's own
             write() contract (fuse_write.cpp: "N bytes written
             (short writes included)"), report the short write
             instead of a bare failure so the kernel/application
             don't believe zero bytes landed when `done` actually
             did. */
          return (done > 0) ? (int)done : rv;
        }
      return (int)(done + rv);
    }

  return (int)copied;
}
