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

#pragma once

#include "base_types.h"
#include "fuse_msgbuf_t.h"

u32  msgbuf_get_pagesize();
void msgbuf_set_bufsize(const u64 size);
u64  msgbuf_get_bufsize();

fuse_msgbuf_t *msgbuf_alloc();
fuse_msgbuf_t *msgbuf_alloc_page_aligned();
void           msgbuf_free(fuse_msgbuf_t *msgbuf);

int  msgbuf_ensure_pipe(fuse_msgbuf_t *msgbuf);
void msgbuf_drain_pipe(fuse_msgbuf_t *msgbuf);

void msgbuf_clear();
void msgbuf_gc();

u64 msgbuf_alloc_count();

/* Shared splice-transport plumbing used by both fuse_msgbuf.cpp and
   fuse_lowlevel.cpp/fuse.cpp - factored here so the two translation
   units can't independently drift on pipe sizing/logging. */

/* Below this size the extra splice/vmsplice syscalls cost more than
   the copy they avoid (benchmark rationale: see src/config.cpp's
   comment on the `splice` option). Shared by the read-reply gate
   (fuse.cpp) and the write/splice-move gate (fuse_lowlevel.cpp) so
   the two thresholds can't silently drift apart. */
constexpr u32 FUSE_SPLICE_MIN_SIZE = 131072u;

/* Read /proc/sys/fs/pipe-max-size once per process (cached) and
   return it, clamped to UINT32_MAX; returns 1048576 (the historical
   assumed default) if the sysctl can't be read or parsed. */
u32 fuse_effective_pipe_max();

enum class splice_fallback_tag_t
  {
    PIPE_MAX_READ_FALLBACK,
    MSGBUF_FSETPIPE_EP,
    MSGBUF_FSETPIPE_ANY,
    MSGBUF_PIPE_CREATE,
    REPLY_PIPE_FSETPIPE_EP,
    REPLY_PIPE_FSETPIPE_ANY,
    REPLY_PIPE_ENSURE,
    REPLY_PIPE_CAP,
    REPLY_VMSPLICE_HDR,
    REPLY_FD_SPLICE,
    RECEIVE_MSGBUF_PIPE,
    RECEIVE_SPLICE_ERR,
    REPLY_SPLICE_MOVE_WRITEV,
  };

/* Warn once per process per tag so a busy mount doesn't spam
   per-request logs when splice degrades to the copy path. */
void fuse_splice_fallback_log(splice_fallback_tag_t tag);

/* Size a pipe's kernel buffer via F_SETPIPE_SZ, clamping the request
   to INT_MAX and retrying once at the 1MiB default on failure -
   unconditionally when retry_unconditionally is set, otherwise only
   when the first attempt failed with EPERM. Logs tag_any once if the
   sized request failed at all, tag_ep once if every attempt failed.
   Returns the granted size (>0) or -errno of the last attempt (<=0). */
int fuse_pipe_set_size(int                   fd,
                       u32                   cap,
                       bool                  retry_unconditionally,
                       splice_fallback_tag_t tag_any,
                       splice_fallback_tag_t tag_ep);

/* Bounded, non-blocking pipe drain: consume exactly the bytes FIONREAD
   reports as queued, reading into scratch (capped at scratch_size),
   retrying on EINTR. Never blocks - the write end is assumed still
   held by the caller, so a blocking read on an already-drained empty
   pipe would wedge the thread forever. No-op if fd is -1. Shared by
   msgbuf_drain_pipe (this file) and _ReplyPipe::drain
   (fuse_lowlevel.cpp) so the receive-side and reply-side pipes can't
   independently drift on drain semantics. */
void fuse_drain_pipe_fd(int fd, void *scratch, size_t scratch_size);
