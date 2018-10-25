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

#include <limits>
#include <string>
#include <vector>

using std::string;
using std::vector;
using mergerfs::Category;

static
int
_eplfs_create(const vector<string>  &basepaths,
              const char            *fusepath,
              const uint64_t         minfreespace,
              vector<const string*> &paths)
{
  int rv;
  uint64_t eplfs;
  fs::info_t info;
  const string *basepath;
  const string *eplfsbasepath;

  eplfs = std::numeric_limits<uint64_t>::max();
  eplfsbasepath = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      basepath = &basepaths[i];

      rv = fs::info(basepath,fusepath,&info);
      if(rv == -1)
        continue;
      if(info.readonly)
        continue;
      if(info.spaceavail < minfreespace)
        continue;
      if(info.spaceavail > eplfs)
        continue;

      eplfs = info.spaceavail;
      eplfsbasepath = basepath;
    }

  if(eplfsbasepath == NULL)
    return (errno=ENOENT,-1);

  paths.push_back(eplfsbasepath);

  return 0;
}

static
int
_eplfs_other(const vector<string>  &basepaths,
             const char            *fusepath,
             vector<const string*> &paths)
{
  int rv;
  uint64_t eplfs;
  uint64_t spaceavail;
  const string *basepath;
  const string *eplfsbasepath;

  eplfs = std::numeric_limits<uint64_t>::max();
  eplfsbasepath = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      basepath = &basepaths[i];

      if(!fs::exists(*basepath,fusepath))
        continue;
      rv = fs::spaceavail(basepath,&spaceavail);
      if(rv == -1)
        continue;
      if(spaceavail > eplfs)
        continue;

      eplfs = spaceavail;
      eplfsbasepath = basepath;
    }

  if(eplfsbasepath == NULL)
    return (errno=ENOENT,-1);

  paths.push_back(eplfsbasepath);

  return 0;
}

static
int
_eplfs(const Category::Enum::Type type,
       const vector<string>       &basepaths,
       const char                 *fusepath,
       const uint64_t              minfreespace,
       vector<const string*>      &paths)
{
  if(type == Category::Enum::create)
    return _eplfs_create(basepaths,fusepath,minfreespace,paths);

  return _eplfs_other(basepaths,fusepath,paths);
}

namespace mergerfs
{
  int
  Policy::Func::eplfs(const Category::Enum::Type  type,
                      const vector<string>       &basepaths,
                      const char                 *fusepath,
                      const uint64_t              minfreespace,
                      vector<const string*>      &paths)
  {
    int rv;

    rv = _eplfs(type,basepaths,fusepath,minfreespace,paths);
    if(rv == -1)
      rv = Policy::Func::lfs(type,basepaths,fusepath,minfreespace,paths);

    return rv;
  }
}
