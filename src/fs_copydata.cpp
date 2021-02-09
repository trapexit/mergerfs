/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_copydata_copy_file_range.hpp"
#include "fs_copydata_readwrite.hpp"
#include "fs_fadvise.hpp"
#include "fs_ficlone.hpp"
#include "fs_ftruncate.hpp"

#include <stddef.h>


namespace fs
{
  int
  copydata(const int    src_fd_,
           const int    dst_fd_,
           const size_t count_)
  {
    int rv;

    rv = fs::ftruncate(dst_fd_,count_);
    if(rv == -1)
      return -1;

    rv = fs::ficlone(src_fd_,dst_fd_);
    if(rv != -1)
      return rv;

    fs::fadvise_willneed(src_fd_,0,count_);
    fs::fadvise_sequential(src_fd_,0,count_);

    rv = fs::copydata_copy_file_range(src_fd_,dst_fd_);
    if(rv != -1)
      return rv;

    return fs::copydata_readwrite(src_fd_,dst_fd_);
  }
}
