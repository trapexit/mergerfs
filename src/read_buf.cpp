/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#if READ_BUF

#include <fuse.h>

#include <stdlib.h>
#include <string.h>

#include "errno.hpp"
#include "fileinfo.hpp"

typedef struct fuse_bufvec fuse_bufvec;

static
int
_read_buf(const int      fd,
          fuse_bufvec  **bufp,
          const size_t   size,
          const off_t    offset)
{
  fuse_bufvec *src;

  src = (fuse_bufvec*)malloc(sizeof(fuse_bufvec));
  if(src == NULL)
    return -ENOMEM;

  *src = FUSE_BUFVEC_INIT(size);

  src->buf->flags = (fuse_buf_flags)(FUSE_BUF_IS_FD|FUSE_BUF_FD_SEEK|FUSE_BUF_FD_RETRY);
  src->buf->fd    = fd;
  src->buf->pos   = offset;

  *bufp = src;

  return 0;
}

namespace mergerfs
{
  namespace fuse
  {
    int
    read_buf(const char      *fusepath,
             fuse_bufvec    **bufp,
             size_t           size,
             off_t            offset,
             fuse_file_info  *ffi)
    {
      FileInfo *fi = reinterpret_cast<FileInfo*>(ffi->fh);

      return _read_buf(fi->fd,
                       bufp,
                       size,
                       offset);
    }
  }
}

#endif
