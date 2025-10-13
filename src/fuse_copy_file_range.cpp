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

#include "fuse_copy_file_range.hpp"

#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_copy_file_range.hpp"

#include "fuse.h"

#include <stdio.h>


static
ssize_t
_copy_file_range(const int src_fd_,
                 off_t     src_off_,
                 const int dst_fd_,
                 off_t     dst_off_,
                 size_t    size_,
                 int       flags_)
{
  ssize_t rv;

  rv = fs::copy_file_range(src_fd_,
                           &src_off_,
                           dst_fd_,
                           &dst_off_,
                           size_,
                           flags_);

  return rv;
}

ssize_t
FUSE::copy_file_range(const fuse_req_ctx_t   *ctx_,
                      const fuse_file_info_t *src_ffi_,
                      off_t                   src_off_,
                      const fuse_file_info_t *dst_ffi_,
                      off_t                   dst_off_,
                      const size_t            size_,
                      const unsigned int      flags_)
{
  FileInfo *src_fi = FileInfo::from_fh(src_ffi_->fh);
  FileInfo *dst_fi = FileInfo::from_fh(dst_ffi_->fh);

  return ::_copy_file_range(src_fi->fd,
                            src_off_,
                            dst_fi->fd,
                            dst_off_,
                            size_,
                            flags_);
}
