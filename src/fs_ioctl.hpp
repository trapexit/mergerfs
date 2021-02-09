/*
  ISC License

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

#pragma once

#include <sys/ioctl.h>


namespace fs
{
  static
  inline
  int
  ioctl(const int           fd_,
        const unsigned long request_)
  {
    return ::ioctl(fd_,request_);
  }

  static
  inline
  int
  ioctl(const int            fd_,
        const unsigned long  request_,
        void                *data_)
  {
    return ::ioctl(fd_,request_,data_);
  }

  static
  inline
  int
  ioctl(const int           fd_,
        const unsigned long request_,
        const int           int_)
  {
    return ::ioctl(fd_,request_,int_);
  }
}
