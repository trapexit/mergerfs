/*
  ISC License

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

#include "fuse_open.hpp"

#include <stdint.h>

#include <stdio.h>

namespace FUSE
{
  int
  prepare_hide(const char *fusepath_,
               uint64_t   *fh_)
  {
    int rv;
    fuse_file_info_t ffi = {0};

    ffi.flags = O_RDONLY|O_NOFOLLOW;
    rv = FUSE::open(fusepath_,&ffi);
    if(rv < 0)
      return rv;

    *fh_ = ffi.fh;

    return 0;
  }
}
