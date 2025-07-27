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

#include "fs_copydata_copy_file_range.hpp"

#include "errno.hpp"
#include "fs_copy_file_range.hpp"
#include "fs_fstat.hpp"

#include "int_types.h"


static
s64
_copydata_copy_file_range(const int src_fd_,
                          const int dst_fd_,
                          const u64 count_)
{
  s64 rv;
  u64 nleft;
  s64 src_off;
  s64 dst_off;

  src_off = 0;
  dst_off = 0;
  nleft   = count_;
  while(nleft > 0)
    {
      rv = fs::copy_file_range(src_fd_,&src_off,dst_fd_,&dst_off,nleft,0);
      switch(rv)
        {
        case -EINTR:
        case -EAGAIN:
          continue;
        case 0:
          break;
        default:
          if(rv < 0)
            return rv;
          break;
        }

      nleft -= rv;
    }

  return (count_ - nleft);
}

s64
fs::copydata_copy_file_range(const int src_fd_,
                             const int dst_fd_,
                             const u64 count_)
{
  return ::_copydata_copy_file_range(src_fd_,
                                     dst_fd_,
                                     count_);
}
