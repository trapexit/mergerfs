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

#include "fasthash.h"

#include <stdint.h>
#include <sys/stat.h>

namespace fs
{
  namespace inode
  {
    static const uint64_t MAGIC = 0x7472617065786974;

    inline
    void
    recompute(struct stat *st_)
    {
      uint64_t buf[5];

      buf[0] = st_->st_ino;
      buf[1] = st_->st_dev;
      buf[2] = buf[0] ^ buf[1];
      buf[3] = buf[0] & buf[1];
      buf[4] = buf[0] | buf[1];

      st_->st_ino = fasthash64(&buf[0],sizeof(buf),MAGIC);
    }
  }
}
