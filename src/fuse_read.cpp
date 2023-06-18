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
#include "fs_pread.hpp"

#include "fuse.h"

#include <stdlib.h>
#include <string.h>


namespace l
{
  static
  int
  read_direct_io(const int     fd_,
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
  read_cached(const int     fd_,
              char         *buf_,
              const size_t  size_,
              const off_t   offset_)
  {
    int rv;

    rv = fs::pread(fd_,buf_,size_,offset_);

    return rv;
  }
}

namespace FUSE
{
  int
  read(const fuse_file_info_t *ffi_,
       char                   *buf_,
       size_t                  size_,
       off_t                   offset_)
  {
    FileInfo *fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    if(fi->direct_io)
      return l::read_direct_io(fi->fd,buf_,size_,offset_);

    return l::read_cached(fi->fd,buf_,size_,offset_);
  }

  int
  read_null(const fuse_file_info_t *ffi_,
            char                   *buf_,
            size_t                  size_,
            off_t                   offset_)
  {
    return size_;
  }
}
