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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "ugid.hpp"
#include "fs.hpp"
#include "config.hpp"
#include "rwlock.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;

static
int
_mknod(Policy::Func::Search  searchFunc,
       Policy::Func::Create  createFunc,
       const vector<string> &srcmounts,
       const size_t          minfreespace,
       const string         &fusepath,
       const mode_t          mode,
       const dev_t           dev)
{
  int rv;
  int error;
  string dirname;
  string fullpath;
  Paths createpaths;
  Paths existingpath;

  dirname = fs::dirname(fusepath);
  rv = searchFunc(srcmounts,dirname,minfreespace,existingpath);
  if(rv == -1)
    return -errno;

  rv = createFunc(srcmounts,dirname,minfreespace,createpaths);
  if(rv == -1)
    return -errno;

  error = 0;
  for(size_t i = 0, ei = createpaths.size(); i != ei; i++)
    {
      const string &createpath = createpaths[0].base;

      if(createpath != existingpath[0].base)
        {
          const mergerfs::ugid::SetResetGuard ugid(0,0);
          fs::clonepath(existingpath[0].base,createpath,dirname);
        }

      fullpath = fs::make_path(createpath,fusepath);

      rv = ::mknod(fullpath.c_str(),mode,dev);
      if(rv == -1)
        error = errno;
    }

  return -error;
}

namespace mergerfs
{
  namespace mknod
  {
    int
    mknod(const char *fusepath,
          mode_t      mode,
          dev_t       rdev)
    {
      const struct fuse_context *fc     = fuse_get_context();
      const config::Config      &config = config::get();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard    readlock(&config.srcmountslock);

      return _mknod(config.getattr,
                    config.mknod,
                    config.srcmounts,
                    config.minfreespace,
                    fusepath,
                    (mode & ~fc->umask),
                    rdev);
    }
  }
}
