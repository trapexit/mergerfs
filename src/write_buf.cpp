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

#include <string>

#include <stdlib.h>
#include <fuse.h>

#include "config.hpp"
#include "fileinfo.hpp"
#include "write.hpp"

using std::string;

static
int
_write_buf_controlfile(const string           controlfile,
                       struct fuse_bufvec    *buf,
                       struct fuse_file_info *fi)
{
  int rv;
  size_t size;
  struct fuse_bufvec dst;

  size = fuse_buf_size(buf);
  dst = FUSE_BUFVEC_INIT(size);
  dst.buf->mem = malloc(size);

  rv = fuse_buf_copy(&dst,buf,(fuse_buf_copy_flags)0);
  if(rv < 0)
    {
      free(dst.buf->mem);
      return rv;
    }

  rv = mergerfs::write::write(controlfile.c_str(),
                              (const char*)dst.buf->mem,
                              size,
                              0,
                              fi);
  free(dst.buf->mem);

  return rv;
}

static
int
_write_buf(const int           fd,
           struct fuse_bufvec *buf,
           const off_t         offset)
{
  size_t size = fuse_buf_size(buf);
  struct fuse_bufvec dest = FUSE_BUFVEC_INIT(size);
  const fuse_buf_copy_flags cpflags =
    (fuse_buf_copy_flags)(FUSE_BUF_SPLICE_MOVE|FUSE_BUF_SPLICE_NONBLOCK);

  dest.buf->flags = (fuse_buf_flags)(FUSE_BUF_IS_FD|FUSE_BUF_FD_SEEK);
  dest.buf->fd    = fd;
  dest.buf->pos   = offset;

  return fuse_buf_copy(&dest,buf,cpflags);
}

namespace mergerfs
{
  namespace write_buf
  {
    int
    write_buf(const char            *fusepath,
              struct fuse_bufvec    *buf,
              off_t                  offset,
              struct fuse_file_info *fi)
    {
      const config::Config &config = config::get();

      if(fusepath == config.controlfile)
        return _write_buf_controlfile(config.controlfile,
                                      buf,
                                      fi);

      return _write_buf(((FileInfo*)fi->fh)->fd,
                        buf,
                        offset);
    }
  }
}
