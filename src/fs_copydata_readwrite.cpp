/*
  Copyright (c) 2018, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_copydata_readwrite.hpp"

#include "errno.hpp"
#include "fs_pread.hpp"
#include "fs_pwriten.hpp"

#include <vector>


static
s64
_copydata_readwrite(const int src_fd_,
                    const int dst_fd_,
                    const u64 count_,
                    const u64 bufsize_)
{
  int err;
  s64 nr;
  s64 nw;
  s64 offset;
  std::vector<char> buf;

  if(bufsize_ == 0)
    return -EINVAL;

  buf.resize(bufsize_);

  offset = 0;
  while(offset < (s64)count_)
    {
      nr = fs::pread(src_fd_,&buf[0],buf.size(),offset);
      switch(nr)
        {
        case -EINTR:
        case -EAGAIN:
          continue;
        case 0:
          return offset;
        default:
          if(nr < 0)
            return nr;
          break;
        }

      nw = fs::pwriten(dst_fd_,&buf[0],nr,offset,&err);
      if(err < 0)
        return err;

      offset += nw;
    }

  return offset;
}

#define BUF_SIZE (256 * 1024)

s64
fs::copydata_readwrite(const int src_fd_,
                       const int dst_fd_,
                       const u64 count_)
{
  return ::_copydata_readwrite(src_fd_,
                               dst_fd_,
                               count_,
                               BUF_SIZE);
}
