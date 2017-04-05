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

#ifndef __FS_INODE_HPP__
#define __FS_INODE_HPP__

#include <sys/stat.h>

namespace fs
{
  namespace inode
  {
    enum
      {
        MAGIC = 0x7472617065786974
      };

    inline
    void
    recompute(struct stat &st)
    {
      /*
       Some OSes have 32-bit device IDs, so box this up first.
       This does also presume a 64-bit inode value.
       */
      uint64_t st_dev = (uint64_t)st.st_dev;
      st.st_ino |= (st_dev << 32);
    }
  }
}

#endif
