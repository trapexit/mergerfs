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
using namespace mergerfs::ugid;

static
int
_single_rename(Policy::Func::Search  searchFunc,
               const vector<string> &srcmounts,
               const size_t          minfreespace,
               const string         &oldbasepath,
               const string         &oldfullpath,
               const string         &newpath)
{
  int rv;
  const string newfullpath = fs::path::make(oldbasepath,newpath);

  rv = ::rename(oldfullpath.c_str(),newfullpath.c_str());
  if(rv == -1 && errno == ENOENT)
    {
      string dirname;
      vector<string> newpathdir;

      dirname = fs::path::dirname(newpath);
      rv = searchFunc(srcmounts,dirname,minfreespace,newpathdir);
      if(rv == -1)
        return -1;

      {
        const SuperUser superuser;
        fs::clonepath(newpathdir[0],oldbasepath,dirname);
      }

      rv = ::rename(oldfullpath.c_str(),newfullpath.c_str());
    }

  return rv;
}

static
int
_rename(Policy::Func::Search  searchFunc,
        Policy::Func::Action  actionFunc,
        const vector<string> &srcmounts,
        const size_t          minfreespace,
        const string         &oldfusepath,
        const string         &newfusepath)
{
  int rv;
  int error;
  vector<string> oldbasepaths;

  rv = actionFunc(srcmounts,oldfusepath,minfreespace,oldbasepaths);
  if(rv == -1)
    return -errno;

  error = 0;
  for(size_t i = 0, ei = oldbasepaths.size(); i != ei; i++)
    {
      const string oldfullpath = fs::path::make(oldbasepaths[i],oldfusepath);

      rv = _single_rename(searchFunc,srcmounts,minfreespace,
                          oldbasepaths[i],oldfullpath,newfusepath);
      if(rv == -1)
        error = errno;
    }

  return -error;
}

namespace mergerfs
{
  namespace fuse
  {
    int
    rename(const char *oldpath,
           const char *newpath)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.srcmountslock);

      return _rename(config.getattr,
                     config.rename,
                     config.srcmounts,
                     config.minfreespace,
                     oldpath,
                     newpath);
    }
  }
}
