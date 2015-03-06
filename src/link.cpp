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
#include <unistd.h>

#include "config.hpp"
#include "fs.hpp"
#include "ugid.hpp"
#include "rwlock.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;

static
int
_single_link(const fs::find::Func  searchFunc,
             const vector<string> &srcmounts,
             const string         &base,
             const string         &oldpath,
             const string         &newpath)
{
  int rv;
  const string fulloldpath = fs::make_path(base,oldpath);
  const string fullnewpath = fs::make_path(base,newpath);

  rv = ::link(fulloldpath.c_str(),fullnewpath.c_str());
  if(rv == -1 && errno == ENOENT)
    {
      string newpathdir;
      fs::Paths foundpath;

      newpathdir = fs::dirname(newpath);
      rv = searchFunc(srcmounts,newpathdir,foundpath,1);
      if(rv == -1)
        return -1;

      {
        const mergerfs::ugid::SetResetGuard ugid(0,0);
        fs::clonepath(foundpath[0].base,base,newpathdir);
      }

      rv = ::link(fulloldpath.c_str(),fullnewpath.c_str());
    }

  return rv;
}

static
int
_link(const fs::find::Func  searchFunc,
      const fs::find::Func  actionFunc,
      const vector<string> &srcmounts,
      const string         &oldpath,
      const string         &newpath)
{
  int rv;
  int error;
  fs::Paths oldpaths;

  rv = actionFunc(srcmounts,oldpath,oldpaths,-1);
  if(rv == -1)
    return -errno;

  error = 0;
  for(fs::Paths::const_iterator
        i = oldpaths.begin(), ei = oldpaths.end(); i != ei; ++i)
    {
      rv = _single_link(searchFunc,srcmounts,i->base,oldpath,newpath);
      if(rv == -1)
        error = errno;
    }

  return -error;
}

namespace mergerfs
{
  namespace link
  {
    int
    link(const char *from,
         const char *to)
    {
      const struct fuse_context *fc     = fuse_get_context();
      const config::Config      &config = config::get();
      const ugid::SetResetGuard  ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard    readlock(&config.srcmountslock);

      return _link(*config.getattr,
                   *config.link,
                   config.srcmounts,
                   from,
                   to);
    }
  }
}
