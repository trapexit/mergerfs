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

#include "fuse_flush.hpp"

#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_close.hpp"
#include "fs_dup.hpp"

#include "fuse.h"


static
int
_flush(const int fd_)
{
  int rv;

  rv = fs::dup(fd_);
  if(rv < 0)
    return -EIO;

  return fs::close(rv);
}

int
FUSE::flush(const fuse_file_info_t *ffi_)
{
  FileInfo *fi = FileInfo::from_fh(ffi_->fh);

  return ::_flush(fi->fd);
}
