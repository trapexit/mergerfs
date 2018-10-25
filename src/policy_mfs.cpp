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

#include "errno.hpp"
#include "fs.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_path.hpp"
#include "policy.hpp"

#include <string>
#include <vector>

using std::string;
using std::vector;
using mergerfs::Category;

static
int
_mfs_create(const vector<string>  &basepaths,
            vector<const string*> &paths)
{
  int rv;
  uint64_t mfs;
  fs::info_t info;
  const string *basepath;
  const string *mfsbasepath;

  mfs = 0;
  mfsbasepath = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      basepath = &basepaths[i];

      rv = fs::info(basepath,&info);
      if(rv == -1)
        continue;
      if(info.readonly)
        continue;
      if(info.spaceavail < mfs)
        continue;

      mfs = info.spaceavail;
      mfsbasepath = basepath;
    }

  if(mfsbasepath == NULL)
    return (errno=ENOENT,-1);

  paths.push_back(mfsbasepath);

  return 0;
}

static
int
_mfs_other(const vector<string>  &basepaths,
           const char            *fusepath,
           vector<const string*> &paths)
{
  int rv;
  uint64_t mfs;
  uint64_t spaceavail;
  const string *basepath;
  const string *mfsbasepath;

  mfs = 0;
  mfsbasepath = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      basepath = &basepaths[i];

      if(!fs::exists(*basepath,fusepath))
        continue;
      rv = fs::spaceavail(basepath,&spaceavail);
      if(rv == -1)
        continue;
      if(spaceavail < mfs)
        continue;

      mfs = spaceavail;
      mfsbasepath = basepath;
    }

  if(mfsbasepath == NULL)
    return (errno=ENOENT,-1);

  paths.push_back(mfsbasepath);

  return 0;
}

static
int
_mfs(const Category::Enum::Type  type,
     const vector<string>       &basepaths,
     const char                 *fusepath,
     vector<const string*>      &paths)
{
  if(type == Category::Enum::create)
    return _mfs_create(basepaths,paths);

  return _mfs_other(basepaths,fusepath,paths);
}

namespace mergerfs
{
  int
  Policy::Func::mfs(const Category::Enum::Type  type,
                    const vector<string>       &basepaths,
                    const char                 *fusepath,
                    const uint64_t              minfreespace,
                    vector<const string*>      &paths)
  {
    int rv;

    rv = _mfs(type,basepaths,fusepath,paths);
    if(rv == -1)
      rv = Policy::Func::ff(type,basepaths,fusepath,minfreespace,paths);

    return rv;
  }
}
