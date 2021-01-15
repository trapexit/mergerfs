/*
  Copyright (c) 2019, Antonio SJ Musumeci <trapexit@spawn.link>

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
#include "fs_copy_file_range.hpp"

#include "fuse.h"

#include <stdio.h>


namespace l
{
  static
  ssize_t
  copy_file_range(const int fd_in_,
                  off_t     offset_in_,
                  const int fd_out_,
                  off_t     offset_out_,
                  size_t    size_,
                  int       flags_)
  {
    ssize_t rv;

    rv = fs::copy_file_range(fd_in_,
                             &offset_in_,
                             fd_out_,
                             &offset_out_,
                             size_,
                             flags_);

    return ((rv == -1) ? -errno : rv);
  }
}

namespace FUSE
{
  ssize_t
  copy_file_range(const fuse_file_info_t *ffi_in_,
                  off_t                   offset_in_,
                  const fuse_file_info_t *ffi_out_,
                  off_t                   offset_out_,
                  size_t                  size_,
                  int                     flags_)
  {
    FileInfo *fi_in  = reinterpret_cast<FileInfo*>(ffi_in_->fh);
    FileInfo *fi_out = reinterpret_cast<FileInfo*>(ffi_out_->fh);

    return l::copy_file_range(fi_in->fd,
                              offset_in_,
                              fi_out->fd,
                              offset_out_,
                              size_,
                              flags_);
  }
}
