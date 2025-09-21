/*
  ISC License

  Copyright (c) 2023, Antonio SJ Musumeci <trapexit@spawn.link>

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

#pragma once

#include "fs_pread.hpp"


namespace fs
{
  static
  inline
  s64
  preadn(const int  fd_,
         void      *buf_,
         const u64  count_,
         const s64  offset_,
         int       *err_)
  {
    s64 rv;
    s64 nleft = count_;
    s64 offset = offset_;
    const char *buf = (const char*)buf_;

    *err_ = 0;
    while(nleft > 0)
      {
        rv = fs::pread(fd_,buf,nleft,offset);
        switch(rv)
          {
          case -EINTR:
          case -EAGAIN:
            continue;
          case 0:
            return (count_ - nleft);
          default:
            if(rv < 0)
              {
                *err_ = rv;
                return (count_ - nleft);
              }
            break;
          }

        buf    += rv;
        nleft  -= rv;
        offset += rv;
      }

    return count_;
  }
}
