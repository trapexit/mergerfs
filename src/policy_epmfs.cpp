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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/statvfs.h>

#include <string>
#include <vector>

#include "policy.hpp"

using std::string;
using std::vector;
using std::size_t;
using mergerfs::Policy;
using mergerfs::Category;
typedef struct statvfs statvfs_t;

static
inline
void
_calc_mfs(const statvfs_t &fsstats,
          const string    &basepath,
          const size_t     minfreespace,
          fsblkcnt_t      &mfs,
          string          &mfsbasepath)
{
  fsblkcnt_t spaceavail;

  spaceavail = (fsstats.f_frsize * fsstats.f_bavail);
  if((spaceavail > minfreespace) && (spaceavail > mfs))
    {
      mfs         = spaceavail;
      mfsbasepath = basepath;
    }
}

static
int
_epmfs_create(const vector<string> &basepaths,
              const string         &fusepath,
              const size_t          minfreespace,
              vector<string>       &paths)

{
  int        rv;
  fsblkcnt_t epmfs;
  string     epmfsbasepath;
  string     fullpath;
  statvfs_t  fsstats;

  epmfs = 0;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      const string &basepath = basepaths[i];

      fs::path::make(basepath,fusepath,fullpath);

      rv = ::statvfs(fullpath.c_str(),&fsstats);
      if(rv == 0)
        _calc_mfs(fsstats,basepath,minfreespace,epmfs,epmfsbasepath);
    }

  if(epmfsbasepath.empty())
    return Policy::Func::mfs(Category::Enum::create,basepaths,fusepath,minfreespace,paths);

  paths.push_back(epmfsbasepath);

  return 0;
}

static
int
_epmfs(const vector<string> &basepaths,
       const string         &fusepath,
       vector<string>       &paths)

{
  int        rv;
  fsblkcnt_t epmfs;
  string     epmfsbasepath;
  string     fullpath;
  statvfs_t  fsstats;

  epmfs = 0;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      const string &basepath = basepaths[i];

      fs::path::make(basepath,fusepath,fullpath);

      rv = ::statvfs(fullpath.c_str(),&fsstats);
      if(rv == 0)
        _calc_mfs(fsstats,basepath,0,epmfs,epmfsbasepath);
    }

  if(epmfsbasepath.empty())
    return (errno=ENOENT,-1);

  paths.push_back(epmfsbasepath);

  return 0;
}

namespace mergerfs
{
  int
  Policy::Func::epmfs(const Category::Enum::Type  type,
                      const vector<string>       &basepaths,
                      const string               &fusepath,
                      const size_t                minfreespace,
                      vector<string>             &paths)
  {
    if(type == Category::Enum::create)
      return _epmfs_create(basepaths,fusepath,minfreespace,paths);

    return _epmfs(basepaths,fusepath,paths);
  }
}
