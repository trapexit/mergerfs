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

#include <fuse.h>

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "config.hpp"
#include "fileinfo.hpp"

static
int
_ftruncate(const int   fd,
           const off_t size)
{
  int rv;

  rv = ::ftruncate(fd,size);

  return ((rv == -1) ? -errno : 0);
}

namespace mergerfs
{
  namespace ftruncate
  {
    int
    ftruncate(const char            *fusepath,
              off_t                  size,
              struct fuse_file_info *fi)
    {
      const config::Config &config   = config::get();
      const FileInfo       *fileinfo = (FileInfo*)fi->fh;

      if(fusepath == config.controlfile)
        return -EPERM;

      return _ftruncate(fileinfo->fd,
                        size);
    }
  }
}
