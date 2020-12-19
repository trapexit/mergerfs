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

#include "errno.hpp"
#include "fs_fstat.hpp"
#include "fs_lseek.hpp"
#include "fs_read.hpp"
#include "fs_write.hpp"

#include <vector>

using std::vector;


namespace l
{
  static
  int
  writen(const int     fd_,
         const char   *buf_,
         const size_t  size_)
  {
    ssize_t rv;
    size_t nleft;

    nleft = size_;
    do
      {
        rv = fs::write(fd_,buf_,nleft);
        if((rv == -1) && (errno == EINTR))
          continue;
        if(rv == -1)
          return -1;

        nleft -= rv;
        buf_  += rv;
      }
    while(nleft > 0);

    return size_;
  }

  static
  int
  copydata_readwrite(const int    src_fd_,
                     const int    dst_fd_,
                     const size_t count_,
                     const size_t blocksize_)
  {
    ssize_t nr;
    ssize_t nw;
    ssize_t bufsize;
    size_t  totalwritten;
    vector<char> buf;

    bufsize = (blocksize_ * 16);
    buf.resize(bufsize);

    fs::lseek(src_fd_,0,SEEK_SET);

    totalwritten = 0;
    while(totalwritten < count_)
      {
        nr = fs::read(src_fd_,&buf[0],bufsize);
        if(nr == 0)
          return totalwritten;
        if((nr == -1) && (errno == EINTR))
          continue;
        if(nr == -1)
          return -1;

        nw = l::writen(dst_fd_,&buf[0],nr);
        if(nw == -1)
          return -1;

        totalwritten += nw;
      }

    return totalwritten;
  }
}

namespace fs
{
  int
  copydata_readwrite(const int src_fd_,
                     const int dst_fd_)
  {
    int rv;
    struct stat st;

    rv = fs::fstat(src_fd_,&st);
    if(rv == -1)
      return -1;

    return l::copydata_readwrite(src_fd_,
                                 dst_fd_,
                                 st.st_size,
                                 st.st_blksize);
  }
}
