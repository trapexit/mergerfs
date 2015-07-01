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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <errno.h>

#include <string>
#include <vector>

#include "policy.hpp"

using std::string;
using std::vector;
using std::size_t;
using mergerfs::Policy;
using mergerfs::Category;

static
int
_lfs_create(const Category::Enum::Type  type,
            const vector<string>       &basepaths,
            const string               &fusepath,
            const size_t                minfreespace,
            Paths                      &paths)
{
  fsblkcnt_t  lfs;
  const char *lfsstr;

  lfs    = -1;
  lfsstr = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      int rv;
      const char *basepath;
      struct statvfs fsstats;

      basepath = basepaths[i].c_str();
      rv = ::statvfs(basepath,&fsstats);
      if(rv == 0)
        {
          fsblkcnt_t spaceavail;

          spaceavail = (fsstats.f_frsize * fsstats.f_bavail);
          if((spaceavail > minfreespace) &&
             (spaceavail < lfs))
            {
              lfs    = spaceavail;
              lfsstr = basepath;
            }
        }
    }

  if(lfsstr == NULL)
    return Policy::Func::mfs(type,basepaths,fusepath,minfreespace,paths);

  paths.push_back(Path(lfsstr,
                       fs::make_path(lfsstr,fusepath)));

  return 0;
}

static
int
_lfs(const Category::Enum::Type  type,
     const vector<string>       &basepaths,
     const string               &fusepath,
     const size_t                minfreespace,
     Paths                      &paths)
{
  fsblkcnt_t  lfs;
  const char *lfsstr;

  lfs    = -1;
  lfsstr = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      int rv;
      string fullpath;
      const char *basepath;
      struct statvfs fsstats;

      basepath = basepaths[i].c_str();
      fullpath = fs::make_path(basepath,fusepath);
      rv = ::statvfs(fullpath.c_str(),&fsstats);
      if(rv == 0)
        {
          fsblkcnt_t spaceavail;

          spaceavail = (fsstats.f_frsize * fsstats.f_bavail);
          if((spaceavail > minfreespace) &&
             (spaceavail < lfs))
            {
              lfs    = spaceavail;
              lfsstr = basepath;
            }
        }
    }

  if(lfsstr == NULL)
    return Policy::Func::mfs(type,basepaths,fusepath,minfreespace,paths);

  paths.push_back(Path(lfsstr,
                       fs::make_path(lfsstr,fusepath)));

  return 0;
}

namespace mergerfs
{
  int
  Policy::Func::lfs(const Category::Enum::Type  type,
                    const vector<string>       &basepaths,
                    const string               &fusepath,
                    const size_t                minfreespace,
                    Paths                      &paths)
  {
    if(type == Category::Enum::create)
      return _lfs_create(type,basepaths,fusepath,minfreespace,paths);

    return _lfs(type,basepaths,fusepath,minfreespace,paths);
  }
}
