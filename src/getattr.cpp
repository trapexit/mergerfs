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

#include <string>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "config.hpp"
#include "fs.hpp"
#include "ugid.hpp"
#include "rwlock.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;

static
int
_getattr_controlfile(struct stat &buf)
{
  time_t now = time(NULL);

  buf.st_dev     = 0;
  buf.st_ino     = 0;
  buf.st_mode    = (S_IFREG|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
  buf.st_nlink   = 1;
  buf.st_uid     = ::getuid();
  buf.st_gid     = ::getgid();
  buf.st_rdev    = 0;
  buf.st_size    = 0;
  buf.st_blksize = 1024;
  buf.st_blocks  = 0;
  buf.st_atime   = now;
  buf.st_mtime   = now;
  buf.st_ctime   = now;

  return 0;
}

static
int
_getattr(const Policy::FuncPtr searchFunc,
         const vector<string> &srcmounts,
         const size_t          minfreespace,
         const string         &fusepath,
         struct stat          &buf)
{
  int rv;
  Paths path;

  rv = searchFunc(srcmounts,fusepath,minfreespace,path);
  if(rv == -1)
    return -errno;

  rv = ::lstat(path[0].full.c_str(),&buf);

  return ((rv == -1) ? -errno : 0);
}

namespace mergerfs
{
  namespace getattr
  {
    int
    getattr(const char  *fusepath,
            struct stat *st)
    {
      const config::Config &config = config::get();

      if(fusepath == config.controlfile)
        return _getattr_controlfile(*st);

      const struct fuse_context *fc = fuse_get_context();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard    readlock(&config.srcmountslock);

      return _getattr(*config.getattr,
                      config.srcmounts,
                      config.minfreespace,
                      fusepath,
                      *st);
    }
  }
}
