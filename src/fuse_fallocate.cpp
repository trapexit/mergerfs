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
#include "fs_fallocate.hpp"

#include <fuse.h>

namespace l
{
  static
  int
  fallocate(const int   fd_,
            const int   mode_,
            const off_t offset_,
            const off_t len_)
  {
    int rv;

    rv = fs::fallocate(fd_,mode_,offset_,len_);

    return ((rv == -1) ? -errno : 0);
  }
}

namespace FUSE
{
  int
  fallocate(const fuse_file_info_t *ffi_,
            int                     mode_,
            off_t                   offset_,
            off_t                   len_)
  {
    FileInfo *fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    return l::fallocate(fi->fd,
                        mode_,
                        offset_,
                        len_);
  }
}
