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

#if WRITE_BUF

#include <fuse.h>

#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_movefile.hpp"
#include "policy.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"
#include "write.hpp"

using std::string;
using std::vector;
using namespace mergerfs;

static
bool
_out_of_space(const int error)
{
  return ((error == ENOSPC) ||
          (error == EDQUOT));
}

static
int
_write_buf(const int    fd,
           fuse_bufvec &src,
           const off_t  offset)
{
  size_t size = fuse_buf_size(&src);
  fuse_bufvec dst  = FUSE_BUFVEC_INIT(size);
  const fuse_buf_copy_flags cpflags =
    (fuse_buf_copy_flags)(FUSE_BUF_SPLICE_MOVE|FUSE_BUF_SPLICE_NONBLOCK);

  dst.buf->flags = (fuse_buf_flags)(FUSE_BUF_IS_FD|FUSE_BUF_FD_SEEK);
  dst.buf->fd    = fd;
  dst.buf->pos   = offset;

  return fuse_buf_copy(&dst,&src,cpflags);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    write_buf(const char     *fusepath,
              fuse_bufvec    *src,
              off_t           offset,
              fuse_file_info *ffi)
    {
      int rv;
      FileInfo *fi = reinterpret_cast<FileInfo*>(ffi->fh);

      rv = _write_buf(fi->fd,*src,offset);
      if(_out_of_space(-rv))
        {
          const fuse_context *fc     = fuse_get_context();
          const Config       &config = Config::get(fc);

          if(config.moveonenospc)
            {
              size_t extra;
              const ugid::Set         ugid(0,0);
              const rwlock::ReadGuard readlock(&config.srcmountslock);

              extra = fuse_buf_size(src);
              rv = fs::movefile(config.srcmounts,fusepath,extra,fi->fd);
              if(rv == -1)
                return -ENOSPC;

              rv = _write_buf(fi->fd,*src,offset);
            }
        }

      return rv;
    }

    int
    write_buf_null(const char     *fusepath,
                   fuse_bufvec    *src,
                   off_t           offset,
                   fuse_file_info *ffi)
    {
      return src->buf[0].size;
    }
  }
}

#endif
