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

#include <string.h>

#include <string>

#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_base_read.hpp"

static
inline
int
_read(const int     fd,
      void         *buf,
      const size_t  count,
      const off_t   offset)
{
  int rv;

  rv = fs::pread(fd,buf,count,offset);
  if(rv == -1)
    return -errno;
  if(rv == 0)
    return 0;

  return count;
}

static
inline
int
_read_direct_io(const int     fd,
                void         *buf,
                const size_t  count,
                const off_t   offset)
{
  int rv;

  rv = fs::pread(fd,buf,count,offset);
  if(rv == -1)
    return -errno;

  return rv;
}

namespace mergerfs
{
  namespace fuse
  {
    int
    read(const char     *fusepath,
         char           *buf,
         size_t          count,
         off_t           offset,
         fuse_file_info *ffi)
    {
      FileInfo *fi = reinterpret_cast<FileInfo*>(ffi->fh);

      return _read(fi->fd,buf,count,offset);
    }

    int
    read_direct_io(const char     *fusepath,
                   char           *buf,
                   size_t          count,
                   off_t           offset,
                   fuse_file_info *ffi)
    {
      FileInfo *fi = reinterpret_cast<FileInfo*>(ffi->fh);

      return _read_direct_io(fi->fd,buf,count,offset);
    }
  }
}
