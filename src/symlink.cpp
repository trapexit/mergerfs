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
#include <string>

#include "fs.hpp"
#include "config.hpp"
#include "ugid.hpp"
#include "rwlock.hpp"

using std::string;
using std::vector;

static
int
_symlink(const vector<string> &srcmounts,
         const string          from,
         const string          to)
{
  int rv;
  fs::PathVector paths;

  fs::find::ff(srcmounts,fs::dirname(to),paths);
  if(paths.empty())
    return -ENOENT;

  paths[0].full = fs::make_path(paths[0].base,to);

  rv = symlink(from.c_str(),paths[0].full.c_str());

  return ((rv == -1) ? -errno : 0);
}

namespace mergerfs
{
  namespace symlink
  {
    int
    symlink(const char *oldpath,
            const char *newpath)
    {
      const struct fuse_context *fc     = fuse_get_context();
      const config::Config      &config = config::get();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard    readlock(&config.srcmountslock);

      return _symlink(config.srcmounts,
                      oldpath,
                      newpath);
    }
  }
}
