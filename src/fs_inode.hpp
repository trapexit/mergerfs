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

#include <stdint.h>
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
      if(sizeof(st.st_ino) == 4)
        st.st_ino |= ((uint32_t)st.st_dev << 16);
      else
        st.st_ino |= ((uint64_t)st.st_dev << 32);
    }
  }
}

#endif
