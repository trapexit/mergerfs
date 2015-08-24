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

static
inline
void
_calc_epmfs(const struct statvfs &fsstats,
            const string         &basepath,
            fsblkcnt_t           &epmfs,
            string               &epmfsbasepath,
            fsblkcnt_t           &mfs,
            string               &mfsbasepath)
{
  fsblkcnt_t spaceavail;

  spaceavail = (fsstats.f_frsize * fsstats.f_bavail);
  if(spaceavail > epmfs)
    {
      epmfs         = spaceavail;
      epmfsbasepath = basepath;
    }

  if(spaceavail > mfs)
    {
      mfs         = spaceavail;
      mfsbasepath = basepath;
    }
}

static
inline
void
_calc_mfs(const struct statvfs &fsstats,
          const string         &basepath,
          fsblkcnt_t           &mfs,
          string               &mfsbasepath)
{
  fsblkcnt_t spaceavail;

  spaceavail = (fsstats.f_frsize * fsstats.f_bavail);
  if(spaceavail > mfs)
    {
      mfs         = spaceavail;
      mfsbasepath = basepath;
    }
}

static
inline
int
_try_statvfs(const string &basepath,
             const string &fullpath,
             fsblkcnt_t   &epmfs,
             string       &epmfsbasepath,
             fsblkcnt_t   &mfs,
             string       &mfsbasepath)
{
  int rv;
  struct statvfs fsstats;

  rv = ::statvfs(fullpath.c_str(),&fsstats);
  if(rv == 0)
    _calc_epmfs(fsstats,basepath,epmfs,epmfsbasepath,mfs,mfsbasepath);

  return rv;
}

static
inline
int
_try_statvfs(const string &basepath,
             fsblkcnt_t   &mfs,
             string       &mfsbasepath)
{
  int rv;
  struct statvfs fsstats;

  rv = ::statvfs(basepath.c_str(),&fsstats);
  if(rv == 0)
    _calc_mfs(fsstats,basepath,mfs,mfsbasepath);

  return rv;
}

static
int
_epmfs_create(const vector<string> &basepaths,
              const string         &fusepath,
              vector<string>       &paths)

{
  fsblkcnt_t epmfs;
  fsblkcnt_t mfs;
  string     mfsbasepath;
  string     epmfsbasepath;
  string     fullpath;

  mfs = 0;
  epmfs = 0;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      int rv;
      const string &basepath = basepaths[i];

      fullpath = fs::path::make(basepath,fusepath);

      rv = _try_statvfs(basepath,fullpath,epmfs,epmfsbasepath,mfs,mfsbasepath);
      if(rv == -1)
        _try_statvfs(basepath,mfs,mfsbasepath);
    }

  if(epmfsbasepath.empty())
    epmfsbasepath = mfsbasepath;

  paths.push_back(epmfsbasepath);

  return 0;
}

static
int
_epmfs(const vector<string> &basepaths,
       const string         &fusepath,
       vector<string>       &paths)

{
  fsblkcnt_t epmfs;
  string     epmfsbasepath;
  string     fullpath;

  epmfs = 0;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      int rv;
      struct statvfs fsstats;
      const string &basepath = basepaths[i];

      fullpath = fs::path::make(basepath,fusepath);

      rv = ::statvfs(fullpath.c_str(),&fsstats);
      if(rv == 0)
        _calc_mfs(fsstats,basepath,epmfs,epmfsbasepath);
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
      return _epmfs_create(basepaths,fusepath,paths);

    return _epmfs(basepaths,fusepath,paths);
  }
}
