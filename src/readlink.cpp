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

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <string>

#include "config.hpp"
#include "ugid.hpp"
#include "fs.hpp"
#include "rwlock.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;

static
int
_readlink(Policy::Func::Search  searchFunc,
          const vector<string> &srcmounts,
          const size_t          minfreespace,
          const string         &fusepath,
          char                 *buf,
          const size_t          size)
{
  int rv;
  vector<string> path;

  rv = searchFunc(srcmounts,fusepath,minfreespace,path);
  if(rv == -1)
    return -errno;

  fs::path::append(path[0],fusepath);

  rv = ::readlink(path[0].c_str(),buf,size);
  if(rv == -1)
    return -errno;

  buf[rv] = '\0';

  return 0;
}

namespace mergerfs
{
  namespace fuse
  {
    int
    readlink(const char *fusepath,
             char       *buf,
             size_t      size)
    {
      const struct fuse_context *fc     = fuse_get_context();
      const config::Config      &config = config::get();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard    readlock(&config.srcmountslock);

      return _readlink(config.readlink,
                       config.srcmounts,
                       config.minfreespace,
                       fusepath,
                       buf,
                       size);
    }
  }
}
