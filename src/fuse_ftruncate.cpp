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
#include "fs_ftruncate.hpp"
#include "state.hpp"

#include "fuse.h"


namespace l
{
  static
  int
  ftruncate(const int   fd_,
            const off_t size_)
  {
    int rv;

    rv = fs::ftruncate(fd_,size_);

    return ((rv == -1) ? -errno : 0);
  }
}

namespace FUSE
{
  int
  ftruncate(const uint64_t fh_,
            off_t          size_)
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
     
    FileInfo *fi = reinterpret_cast<FileInfo*>(fh);

    return l::ftruncate(fi->fd,size_);
  }
}
