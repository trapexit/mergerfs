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

#include "errno.hpp"
#include "fileinfo.hpp"

#include <fuse.h>

#include <stdlib.h>
#include <string.h>

typedef struct fuse_bufvec fuse_bufvec;

namespace l
{
  static
  int
  read_buf(const int      fd_,
           fuse_bufvec  **bufp_,
           const size_t   size_,
           const off_t    offset_)
  {
    fuse_bufvec *src;

    src = (fuse_bufvec*)malloc(sizeof(fuse_bufvec));
    if(src == NULL)
      return -ENOMEM;

    *src = FUSE_BUFVEC_INIT(size_);

    src->buf->flags = (fuse_buf_flags)(FUSE_BUF_IS_FD|FUSE_BUF_FD_SEEK|FUSE_BUF_FD_RETRY);
    src->buf->fd    = fd_;
    src->buf->pos   = offset_;

    *bufp_ = src;

    return 0;
  }
}

namespace FUSE
{
  int
  read_buf(const fuse_file_info_t  *ffi_,
           fuse_bufvec            **bufp_,
           size_t                   size_,
           off_t                    offset_)
  {
    FileInfo *fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    return l::read_buf(fi->fd,
                     bufp_,
                     size_,
                     offset_);
  }
}
