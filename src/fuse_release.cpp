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

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_close.hpp"
#include "fs_fadvise.hpp"

#include "fuse.h"

#include <string>


namespace l
{
  static
  int
  release(FileInfo   *fi_,
          const bool  dropcacheonclose_)
  {
    // according to Feh of nocache calling it once doesn't always work
    // https://github.com/Feh/nocache
    if(dropcacheonclose_)
      {
        fs::fadvise_dontneed(fi_->fd);
        fs::fadvise_dontneed(fi_->fd);
      }

    fs::close(fi_->fd);

    delete fi_;

    return 0;
  }
}

namespace FUSE
{
  int
  release(const fuse_file_info_t *ffi_)
  {
    Config::Read cfg;
    FileInfo *fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    return l::release(fi,cfg->dropcacheonclose);
  }
}
