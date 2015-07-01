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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

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
_single_rename(Policy::Func::Search  searchFunc,
               const vector<string> &srcmounts,
               const size_t          minfreespace,
               const Path           &oldpath,
               const string         &newpath)
{
  int rv;
  const string fullnewpath = fs::make_path(oldpath.base,newpath);

  rv = ::rename(oldpath.full.c_str(),fullnewpath.c_str());
  if(rv == -1 && errno == ENOENT)
    {
      string dirname;
      Paths newpathdir;

      dirname = fs::dirname(newpath);
      rv = searchFunc(srcmounts,dirname,minfreespace,newpathdir);
      if(rv == -1)
        return -1;

      {
        const mergerfs::ugid::SetResetGuard ugid(0,0);
        fs::clonepath(newpathdir[0].base,oldpath.base,dirname);
      }

      rv = ::rename(oldpath.full.c_str(),fullnewpath.c_str());
    }

  return rv;
}

static
int
_rename(Policy::Func::Search  searchFunc,
        Policy::Func::Action  actionFunc,
        const vector<string> &srcmounts,
        const size_t          minfreespace,
        const string         &oldpath,
        const string         &newpath)
{
  int rv;
  int error;
  Paths oldpaths;

  rv = actionFunc(srcmounts,oldpath,minfreespace,oldpaths);
  if(rv == -1)
    return -errno;

  error = 0;
  for(size_t i = 0, ei = oldpaths.size(); i != ei; i++)
    {
      rv = _single_rename(searchFunc,srcmounts,minfreespace,oldpaths[i],newpath);
      if(rv == -1)
        error = errno;
    }

  return -error;
}

namespace mergerfs
{
  namespace rename
  {
    int
    rename(const char *oldpath,
           const char *newpath)
    {
      const struct fuse_context *fc     = fuse_get_context();
      const config::Config      &config = config::get();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard    readlock(&config.srcmountslock);

      return _rename(config.getattr,
                     config.rename,
                     config.srcmounts,
                     config.minfreespace,
                     oldpath,
                     newpath);
    }
  }
}
