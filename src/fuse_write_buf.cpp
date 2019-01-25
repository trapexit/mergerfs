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

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_movefile.hpp"
#include "fuse_write.hpp"
#include "policy.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

#include <fuse.h>

#include <string>
#include <vector>

#include <stdlib.h>
#include <unistd.h>

using std::string;
using std::vector;

namespace l
{
  static
  bool
  out_of_space(const int error_)
  {
    return ((error_ == ENOSPC) ||
            (error_ == EDQUOT));
  }

  static
  int
  write_buf(const int    fd_,
            fuse_bufvec *src_,
            const off_t  offset_)
  {
    size_t size = fuse_buf_size(src_);
    fuse_bufvec dst = FUSE_BUFVEC_INIT(size);
    const fuse_buf_copy_flags cpflags =
      (fuse_buf_copy_flags)(FUSE_BUF_SPLICE_MOVE|FUSE_BUF_SPLICE_NONBLOCK);

    dst.buf->flags = (fuse_buf_flags)(FUSE_BUF_IS_FD|FUSE_BUF_FD_SEEK);
    dst.buf->fd    = fd_;
    dst.buf->pos   = offset_;

    return fuse_buf_copy(&dst,src_,cpflags);
  }
}

namespace FUSE
{
  int
  write_buf(const char     *fusepath_,
            fuse_bufvec    *src_,
            off_t           offset_,
            fuse_file_info *ffi_)
  {
    int rv;
    FileInfo *fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    rv = l::write_buf(fi->fd,src_,offset_);
    if(l::out_of_space(-rv))
      {
        const fuse_context *fc     = fuse_get_context();
        const Config       &config = Config::get(fc);

        if(config.moveonenospc)
          {
            size_t extra;
            vector<string> paths;
            const ugid::Set ugid(0,0);
            const rwlock::ReadGuard readlock(&config.branches_lock);

            config.branches.to_paths(paths);

            extra = fuse_buf_size(src_);
            rv = fs::movefile(paths,fi->fusepath,extra,fi->fd);
            if(rv == -1)
              return -ENOSPC;

            rv = l::write_buf(fi->fd,src_,offset_);
          }
      }

    return rv;
  }

  int
  write_buf_null(const char     *fusepath_,
                 fuse_bufvec    *src_,
                 off_t           offset_,
                 fuse_file_info *ffi_)
  {
    return src_->buf[0].size;
  }
}
