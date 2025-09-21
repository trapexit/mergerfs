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

#include "fuse_futimens.hpp"

#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_futimens.hpp"
#include "state.hpp"

#include "fuse.h"

#include <sys/stat.h>


static
int
_futimens(const int             fd_,
          const struct timespec ts_[2])
{
  int rv;

  rv = fs::futimens(fd_,ts_);

  return rv;
}

int
FUSE::futimens(const uint64_t        fh_,
               const struct timespec ts_[2])
{
  uint64_t fh;
  const fuse_context *fc = fuse_get_context();

  fh = fh_;
  if(fh == 0)
    {
      state.open_files.cvisit(fc->nodeid,
                              [&](const auto &val_)
                              {
                                fh = val_.second.fi->to_fh();
                              });
    }

  FileInfo *fi = FileInfo::from_fh(fh);

  return ::_futimens(fi->fd,ts_);
}
