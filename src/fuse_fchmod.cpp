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

#include "fuse_fchmod.hpp"

#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_fchmod.hpp"
#include "state.hpp"

#include "fuse.h"


static
int
_fchmod(const int    fd_,
        const mode_t mode_)
{
  int rv;

  rv = fs::fchmod(fd_,mode_);

  return rv;
}

int
FUSE::fchmod(const uint64_t fh_,
             const mode_t   mode_)
{
  uint64_t fh;
  const fuse_context *fc = fuse_get_context();

  fh = fh_;
  if(fh == 0)
    {
      state.open_files.cvisit(fc->nodeid,
                              [&](auto &val_)
                              {
                                fh = reinterpret_cast<uint64_t>(val_.second.fi);
                              });
    }

  if(fh == 0)
    return -ENOENT;

  FileInfo *fi = reinterpret_cast<FileInfo*>(fh);

  return ::_fchmod(fi->fd,mode_);
}
