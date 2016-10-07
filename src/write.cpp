/*
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

#include <fuse.h>

#include <unistd.h>

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_movefile.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

static
bool
_out_of_space(const int error)
{
  return ((error == ENOSPC) ||
          (error == EDQUOT));
}

static
int
_write(const int     fd,
       const void   *buf,
       const size_t  count,
       const off_t   offset)
{
  int rv;

  rv = ::pwrite(fd,buf,count,offset);

  return ((rv == -1) ? -errno : rv);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    write(const char     *fusepath,
          const char     *buf,
          size_t          count,
          off_t           offset,
          fuse_file_info *ffi)
    {
      int rv;
      FileInfo* fi = reinterpret_cast<FileInfo*>(ffi->fh);

      rv = _write(fi->fd,buf,count,offset);
      if(_out_of_space(-rv))
        {
          const fuse_context *fc     = fuse_get_context();
          const Config       &config = Config::get(fc);

          if(config.moveonenospc)
            {
              const ugid::Set         ugid(0,0);
              const rwlock::ReadGuard readlock(&config.srcmountslock);

              rv = fs::movefile(config.srcmounts,fusepath,count,fi->fd);
              if(rv == -1)
                return -ENOSPC;

              rv = _write(fi->fd,buf,count,offset);
            }
        }

      return rv;
    }
  }
}
