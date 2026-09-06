/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_read_buf.hpp"

#include "fileinfo.hpp"
#include "ioprio.hpp"
#include "state.hpp"

#include "fuse.h"
#include "fuse_common.h"


/* Return a bufvec referencing the branch fd at the read offset. The
   libfuse fuse_lib_read path splices payload directly fd->pipe->/dev/fuse
   (kernel-side), avoiding a userspace copy of the payload. Called only
   when splice is on and the request size is >= 128KiB (see
   fuse_lib_read in fuse.cpp); the payload's memory residency is
   irrelevant in that path so we never pread.
*/
int
FUSE::read_buf(const fuse_req_ctx_t   *ctx_,
               const fuse_file_info_t *ffi_,
               fuse_bufvec            *bufp_,
               const size_t            size_,
               const off_t             offset_)
{
  FileInfo *fi;

  /* ioprio is thread-persistent; without SetFrom the branch-file I/O
     performed downstream of this handler (the kernel-side splice in
     fuse_reply_data_splice_fd runs on this same worker thread) would
     inherit whatever priority a previous request left behind. */
  ioprio::SetFrom iop(ctx_->pid);

  fi = state.get_fi(ctx_,ffi_->fh);
  if(not fi)
    return -EBADF;

  /* ffi_->flags arrives verbatim from the client's FUSE_OPEN and is
     passed through to fs::openat, so a client O_DIRECT open yields an
     O_DIRECT branch fd. That is still safe here: a splice that
     rejects it (EINVAL) or hits EOF (0) returns -1 from
     fuse_reply_data_splice_fd and fuse_lib_read_fd_splice falls back
     to the plain pread read path. */

  *bufp_ = fuse_bufvec_fd(fi->fd,offset_,size_,
                         (fuse_buf_flags)(FUSE_BUF_IS_FD|
                                          FUSE_BUF_FD_SEEK|
                                          FUSE_BUF_FD_RETRY));

  return 0;
}
