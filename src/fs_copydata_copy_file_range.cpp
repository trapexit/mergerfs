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
#include "fs_copy_file_range.hpp"
#include "fs_fstat.hpp"

#include <cstdint>


namespace l
{
  int64_t
  copydata_copy_file_range(const int src_fd_,
                           const int dst_fd_,
                           uint64_t  size_)
  {
    int64_t  rv;
    uint64_t nleft;
    int64_t  src_off;
    int64_t  dst_off;

    src_off = 0;
    dst_off = 0;
    nleft   = size_;
    do
      {
        rv = fs::copy_file_range(src_fd_,&src_off,dst_fd_,&dst_off,nleft,0);
        if((rv == -1) && (errno == EINTR))
          continue;
        if(rv == -1)
          return -1;

        nleft -= rv;
      }
    while(nleft > 0);

    return size_;
  }
}

namespace fs
{
  int64_t
  copydata_copy_file_range(const int src_fd_,
                           const int dst_fd_)
  {
    int rv;
    struct stat st;

    rv = fs::fstat(src_fd_,&st);
    if(rv == -1)
      return -1;

    return l::copydata_copy_file_range(src_fd_,
                                       dst_fd_,
                                       st.st_size);
  }
}
