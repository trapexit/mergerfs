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
#include "fs_fchmod.hpp"

#include <fuse.h>

namespace l
{
  static
  int
  fchmod(const int    fd_,
         const mode_t mode_)
  {
    int rv;

    rv = fs::fchmod(fd_,mode_);
    if(rv == -1)
      return -errno;

    return rv;
  }
}

namespace FUSE
{
  int
  fchmod(const fuse_file_info_t *ffi_,
         const mode_t            mode_)
  {
    FileInfo *fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    return l::fchmod(fi->fd,mode_);
  }
}
