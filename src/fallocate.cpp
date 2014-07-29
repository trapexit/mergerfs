/*
   The MIT License (MIT)

   Copyright (c) 2014 Antonio SJ Musumeci <trapexit@spawn.link>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fuse.h>

#include <errno.h>
#include <fcntl.h>

#include "fileinfo.hpp"

static
int
_fallocate(const int   fd,
           const int   mode,
           const off_t offset,
           const off_t len)
{
  int rv;

#ifdef _GNU_SOURCE
  rv = ::fallocate(fd,mode,offset,len);
#elif defined _XOPEN_SOURCE >= 600 || _POSIX_C_SOURCE >= 200112L
  if(mode)
    {
      rv = -1;
      errno = EOPNOTSUPP;
    }
  else
    {
      rv = ::posix_fallocate(fd,offset,len);
    }
#else
  rv = -1;
  errno = EOPNOTSUPP;
#endif

  return ((rv == -1) ? -errno : 0);
}

namespace mergerfs
{
  namespace fallocate
  {
    int
    fallocate(const char            *fusepath,
              int                    mode,
              off_t                  offset,
              off_t                  len,
              struct fuse_file_info *ffi)
    {
      const FileInfo *fileinfo = (FileInfo*)ffi->fh;

      return _fallocate(fileinfo->fd,
                        mode,
                        offset,
                        len);
    }
  }
}
