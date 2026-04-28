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

#include "fuse_fallocate.hpp"

#include "state.hpp"

#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_fallocate.hpp"

#include "fuse.h"


static
int
_fallocate(const int   fd_,
           const int   mode_,
           const off_t offset_,
           const off_t len_)
{
  int rv;

  rv = fs::fallocate(fd_,mode_,offset_,len_);

  return rv;
}

// In cases where the node was opened but unlinked there will be no
// path and no guarentee of a `fh`. It could always do the lookup but
// why bother if the kernel has provided it to us? The reason the
// function doesn't need to run within the visit lambda is because
// since the request is outstanding the kernel won't be releasing the
// node and therefore the entry will be valid over the lifetime of
// this function.
int
FUSE::fallocate(const fuse_req_ctx_t *ctx_,
                cu64                  fh_,
                int                   mode_,
                off_t                 offset_,
                off_t                 len_)
{
  FileInfo *fi;

  fi = state.get_fi(ctx_,fh_);
  if(not fi)
    return -EBADF;

  return ::_fallocate(fi->fd,
                      mode_,
                      offset_,
                      len_);
}
