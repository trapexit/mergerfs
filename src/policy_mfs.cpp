/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <sys/statvfs.h>
#include <errno.h>

#include <string>
#include <vector>

#include "fs_path.hpp"
#include "policy.hpp"

using std::string;
using std::vector;
using std::size_t;

static
int
_mfs_create(const vector<string> &basepaths,
            const string         &fusepath,
            vector<string>       &paths)
{
  fsblkcnt_t mfs;
  string     mfsstr;

  mfs = 0;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      int rv;
      struct statvfs fsstats;
      const string &basepath = basepaths[i];

      rv = ::statvfs(basepath.c_str(),&fsstats);
      if(rv == 0)
        {
          fsblkcnt_t spaceavail;

          spaceavail = (fsstats.f_frsize * fsstats.f_bavail);
          if(spaceavail > mfs)
            {
              mfs    = spaceavail;
              mfsstr = basepath;
            }
        }
    }

  if(mfsstr.empty())
    return (errno=ENOENT,-1);

  paths.push_back(mfsstr);

  return 0;
}

static
int
_mfs(const vector<string> &basepaths,
     const string         &fusepath,
     vector<string>       &paths)
{
  fsblkcnt_t mfs;
  string     mfsstr;

  mfs = 0;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      int rv;
      string fullpath;
      struct statvfs fsstats;
      const string &basepath = basepaths[i];

      fs::path::make(basepath,fusepath,fullpath);

      rv = ::statvfs(fullpath.c_str(),&fsstats);
      if(rv == 0)
        {
          fsblkcnt_t spaceavail;

          spaceavail = (fsstats.f_frsize * fsstats.f_bavail);
          if(spaceavail > mfs)
            {
              mfs    = spaceavail;
              mfsstr = basepath;
            }
        }
    }

  if(mfsstr.empty())
    return (errno=ENOENT,-1);

  paths.push_back(mfsstr);

  return 0;
}

namespace mergerfs
{
  int
  Policy::Func::mfs(const Category::Enum::Type  type,
                    const vector<string>       &basepaths,
                    const string               &fusepath,
                    const size_t                minfreespace,
                    vector<string>             &paths)
  {
    if(type == Category::Enum::create)
      return _mfs_create(basepaths,fusepath,paths);

    return _mfs(basepaths,fusepath,paths);
  }
}
