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
#include "fs_fstat.hpp"
#include "fs_lseek.hpp"
#include "fs_read.hpp"
#include "fs_write.hpp"

#include <vector>


static
s64
_writen(const int   fd_,
        const char *buf_,
        const u64   size_)
{
  s64 rv;
  u64 nleft;

  nleft = size_;
  do
    {
      rv = fs::write(fd_,buf_,nleft);
      if(rv == -EINTR)
        continue;
      if(rv == -EAGAIN)
        continue;
      if(rv == 0)
        break;
      if(rv < 0)
        return rv;

      nleft -= rv;
      buf_  += rv;
    }
  while(nleft > 0);

  return (size_ - nleft);
}

static
s64
_copydata_readwrite(const int src_fd_,
                    const int dst_fd_,
                    const u64 count_)
{
  s64 nr;
  s64 nw;
  s64 bufsize;
  u64 totalwritten;
  std::vector<char> buf;

  bufsize = (1024 * 1024);
  buf.resize(bufsize);

  totalwritten = 0;
  while(totalwritten < count_)
    {
      nr = fs::read(src_fd_,&buf[0],bufsize);
      if(nr == 0)
        return totalwritten;
      if(nr == -EINTR)
        continue;
      if(nr == -EAGAIN)
        continue;
      if(nr < 0)
        return nr;

      nw = ::_writen(dst_fd_,&buf[0],nr);
      if(nw < 0)
        return nw;

      totalwritten += nw;
    }

  return totalwritten;
}


s64
fs::copydata_readwrite(const int src_fd_,
                       const int dst_fd_,
                       const u64 count_)
{
  return ::_copydata_readwrite(src_fd_,
                               dst_fd_,
                               count_);
}
