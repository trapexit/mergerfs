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

#include "fuse_read.hpp"

#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_pread.hpp"
#include "ioprio.hpp"
#include "state.hpp"

#include "fuse.h"

#include <stdlib.h>
#include <string.h>


static
int
_read_direct_io(const int     fd_,
                char         *buf_,
                const size_t  size_,
                const off_t   offset_)
{
  int rv;

  rv = fs::pread(fd_,buf_,size_,offset_);

  return rv;
}

static
int
_read_cached(const int     fd_,
             char         *buf_,
             const size_t  size_,
             const off_t   offset_)
{
  int rv;

  rv = fs::pread(fd_,buf_,size_,offset_);

  return rv;
}

int
FUSE::read(const fuse_req_ctx_t   *ctx_,
           const fuse_file_info_t *ffi_,
           char                   *buf_,
           size_t                  size_,
           off_t                   offset_)
{
  ioprio::SetFrom iop(ctx_->pid);
  FileInfo *fi;

  fi = state.get_fi(ctx_,ffi_->fh);
  if(not fi)
    return -EBADF;

  const int rv = (fi->direct_io ?
                  ::_read_direct_io(fi->fd,buf_,size_,offset_) :
                  ::_read_cached(fi->fd,buf_,size_,offset_));

  /* A short read means end-of-file at offset+rv: keep read_buf's size
     floor from claiming more (it is the splice reply's EOF fallback).
     Nothing was read at all when rv is 0, so the file may end anywhere
     at or before offset_ (it may have shrunk well below the offset
     asked about): only 0 is then a bound that cannot be too high. */
  if((rv >= 0) && ((size_t)rv < size_))
    fi->size_floor_lower((rv > 0) ? ((s64)offset_ + rv) : 0);

  return rv;
}

int
FUSE::read_null(const fuse_req_ctx_t   *ctx_,
                const fuse_file_info_t *ffi_,
                char                   *buf_,
                size_t                  size_,
                off_t                   offset_)
{
  return size_;
}
