/*
  Copyright (c) 2017, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_fsyncdir.hpp"

#include "errno.hpp"
#include "dirinfo.hpp"
#include "fs_fsync.hpp"

#include "fuse.h"

#include <string>
#include <vector>


static
int
_fsyncdir(const DirInfo *di_,
          const int      isdatasync_)
{
  return -ENOSYS;
}

int
FUSE::fsyncdir(const fuse_file_info_t *ffi_,
               int                     isdatasync_)
{
  DirInfo *di = reinterpret_cast<DirInfo*>(ffi_->fh);

  return ::_fsyncdir(di,isdatasync_);
}
