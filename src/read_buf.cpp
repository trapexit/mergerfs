/*
   The MIT License (MIT)

   Copyright (c) 2014 Antonio SJ Musumeci <trapexit@spawn.link>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#if READ_BUF

#include <fuse.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "fileinfo.hpp"

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
