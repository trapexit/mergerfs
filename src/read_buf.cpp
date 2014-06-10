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

#include <fuse.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "config.hpp"
#include "ugid.hpp"
#include "fileinfo.hpp"

using std::string;

static
void *
memdup(const void   *src,
       const size_t  len)
{
  void *dst;

  dst = malloc(len);
  if(dst == NULL)
    return NULL;

  memcpy(dst, src, len);

  return dst;
}

static
int
_read_buf_controlfile(const string         readstr,
                      struct fuse_bufvec **bufp,
                      const size_t         size)
{
  struct fuse_bufvec *src;

  src = (fuse_bufvec*)malloc(sizeof(struct fuse_bufvec));
  if(src == NULL)
    return -ENOMEM;

  *src = FUSE_BUFVEC_INIT(std::min(size,readstr.size()));

  src->buf->mem = memdup(readstr.data(),readstr.size());
  if(src->buf->mem == NULL)
    return -ENOMEM;

  *bufp = src;

  return 0;
}

static
int
_read_buf(const int            fd,
          struct fuse_bufvec **bufp,
          const size_t         size,
          const off_t          offset)
{
  struct fuse_bufvec *src;

  src = (fuse_bufvec*)malloc(sizeof(struct fuse_bufvec));
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
  namespace read_buf
  {
    int
    read_buf(const char             *fusepath,
             struct fuse_bufvec    **bufp,
             size_t                  size,
             off_t                   offset,
             struct fuse_file_info  *fi)
    {
      const ugid::SetResetGuard  ugid;
      const config::Config      &config = config::get();

      if(fusepath == config.controlfile)
        return _read_buf_controlfile(config.readstr,
                                     bufp,
                                     size);

      return _read_buf(((FileInfo*)fi->fh)->fd,
                       bufp,
                       size,
                       offset);
    }
  }
}
