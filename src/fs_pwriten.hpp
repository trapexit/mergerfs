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

#include "fs_pwrite.hpp"


namespace fs
{
  static
  inline
  ssize_t
  pwriten(const int     fd_,
          const void   *buf_,
          const size_t  count_,
          const off_t   offset_,
          int          *err_)
  {
    ssize_t rv;
    ssize_t count  = count_;
    off_t   offset = offset_;
    char const *buf = (char const *)buf_;

    *err_ = 0;
    while(count > 0)
      {
        rv = fs::pwrite(fd_,buf,count,offset);
        if(rv == 0)
          return (count_ - count);
        if(rv < 0)
          {
            *err_ = rv;
            return (count_ - count);
          }

        buf    += rv;
        count  -= rv;
        offset += rv;
      }

    return count_;
  }
}
