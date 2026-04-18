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

#include "fuse_ftruncate.hpp"

#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_ftruncate.hpp"
#include "state.hpp"

#include "fuse.h"


static
int
_ftruncate(const int   fd_,
           const off_t size_)
{
  int rv;

  rv = fs::ftruncate(fd_,size_);

  return rv;
}

int
FUSE::ftruncate(const fuse_req_ctx_t *ctx_,
                cu64                  fh_,
                off_t                 size_)
{
  FileInfo *fi = FileInfo::from_fh(fh_);

  if(not fi)
    return -EBADF;

  return ::_ftruncate(fi->fd,size_);
}
