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
#include <set>

#include "ugid.hpp"
#include "fs.hpp"
#include "config.hpp"
#include "rwlock.hpp"

using std::string;
using std::vector;
using std::set;
using mergerfs::Policy;

static
bool
_different_dirname(const string &path0,
                   const string &path1)
{
  return (fs::path::dirname(path0) != fs::path::dirname(path1));
}

static
void
_unlink(const set<string> &tounlink,
        const string      &newfusepath)
{
  int rv;
  string fullpath;

  for(set<string>::const_iterator i = tounlink.begin(), ei = tounlink.end(); i != ei; i++)
    {
      fs::path::make(*i,newfusepath,fullpath);

      rv = ::unlink(fullpath.c_str());
      if(rv == -1 && errno == EISDIR)
        ::rmdir(fullpath.c_str());
    }
}

static
int
_rename(const vector<string> &srcmounts,
        const vector<string> &basepaths,
        const string         &oldfusepath,
        const string         &newfusepath)
{
  int rv;
  int error;
  string oldfullpath;
  string newfullpath;
  set<string> tounlink;

  error = 0;
  tounlink.insert(srcmounts.begin(),srcmounts.end());
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      fs::path::make(basepaths[i],oldfusepath,oldfullpath);
      fs::path::make(basepaths[i],newfusepath,newfullpath);

      tounlink.erase(basepaths[i]);
      rv = ::rename(oldfullpath.c_str(),newfullpath.c_str());
      if(rv == -1)
        error = errno;
    }

  if(error == 0)
    _unlink(tounlink,newfusepath);

  return -error;
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
  vector<string> oldbasepaths;

  if(_different_dirname(oldfusepath,newfusepath))
    return -EXDEV;

  rv = actionFunc(srcmounts,oldfusepath,minfreespace,oldbasepaths);
  if(rv == -1)
    return -errno;

  return _rename(srcmounts,oldbasepaths,oldfusepath,newfusepath);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    rename(const char *oldpath,
           const char *newpath)
    {
      const fuse_context        *fc     = fuse_get_context();
      const Config              &config = Config::get(fc);
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
