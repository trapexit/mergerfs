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
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "ugid.hpp"
#include "fs.hpp"
#include "config.hpp"
#include "rwlock.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;

static
int
_mkdir(const Policy::Func::Ptr  searchFunc,
       const Policy::Func::Ptr  createFunc,
       const vector<string>    &srcmounts,
       const size_t             minfreespace,
       const string            &fusepath,
       const mode_t             mode)
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
  for(Paths::const_iterator
        i = createpaths.begin(), ei = createpaths.end(); i != ei; ++i)
    {
      if(i->base != existingpath[0].base)
        {
          const mergerfs::ugid::SetResetGuard ugid(0,0);
          fs::clonepath(existingpath[0].base,i->base,dirname);
        }

      fullpath = fs::make_path(i->base,fusepath);

      rv = ::mkdir(fullpath.c_str(),mode);
      if(rv == -1)
        error = errno;
    }

  return -error;
}

namespace mergerfs
{
  namespace mkdir
  {
    int
    mkdir(const char *fusepath,
          mode_t      mode)
    {
      const struct fuse_context *fc     = fuse_get_context();
      const config::Config      &config = config::get();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard    readlock(&config.srcmountslock);

      return _mkdir(*config.getattr,
                    *config.mkdir,
                    config.srcmounts,
                    config.minfreespace,
                    fusepath,
                    (mode & ~fc->umask));
    }
  }
}
