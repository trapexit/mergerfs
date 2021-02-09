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
#include "fs_read.hpp"

#include "fuse.h"


namespace l
{
  static
  inline
  int
  read_regular(const int     fd_,
               void         *buf_,
               const size_t  count_,
               const off_t   offset_)
  {
    int rv;

    rv = fs::pread(fd_,buf_,count_,offset_);
    if(rv == -1)
      return -errno;
    if(rv == 0)
      return 0;

    return count_;
  }

  static
  inline
  int
  read_direct_io(const int     fd_,
                 void         *buf_,
                 const size_t  count_,
                 const off_t   offset_)
  {
    int rv;

    rv = fs::pread(fd_,buf_,count_,offset_);
    if(rv == -1)
      return -errno;

    return rv;
  }
}

namespace FUSE
{
  int
  read(const fuse_file_info_t *ffi_,
       char                   *buf_,
       size_t                  count_,
       off_t                   offset_)
  {
    FileInfo *fi;

    fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    if(ffi_->direct_io)
      return l::read_direct_io(fi->fd,buf_,count_,offset_);
    return l::read_regular(fi->fd,buf_,count_,offset_);
  }

  int
  read_null(const fuse_file_info_t *ffi_,
            char                   *buf_,
            size_t                  count_,
            off_t                   offset_)
  {
    return count_;
  }
}
