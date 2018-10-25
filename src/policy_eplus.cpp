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
_eplus_create(const vector<string>  &basepaths,
              const char            *fusepath,
              const uint64_t         minfreespace,
              vector<const string*> &paths)
{
  int rv;
  uint64_t eplus;
  fs::info_t info;
  const string *basepath;
  const string *eplusbasepath;

  eplus = std::numeric_limits<uint64_t>::max();
  eplusbasepath = NULL;
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
      if(info.spaceused >= eplus)
        continue;

      eplus = info.spaceused;
      eplusbasepath = basepath;
    }

  if(eplusbasepath == NULL)
    return (errno=ENOENT,-1);

  paths.push_back(eplusbasepath);

  return 0;
}

static
int
_eplus_other(const vector<string>  &basepaths,
             const char            *fusepath,
             vector<const string*> &paths)
{
  int rv;
  uint64_t eplus;
  uint64_t spaceused;
  const string *basepath;
  const string *eplusbasepath;

  eplus = 0;
  eplusbasepath = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      basepath = &basepaths[i];

      if(!fs::exists(*basepath,fusepath))
        continue;
      rv = fs::spaceused(basepath,&spaceused);
      if(rv == -1)
        continue;
      if(spaceused >= eplus)
        continue;

      eplus = spaceused;
      eplusbasepath = basepath;
    }

  if(eplusbasepath == NULL)
    return (errno=ENOENT,-1);

  paths.push_back(eplusbasepath);

  return 0;
}

static
int
_eplus(const Category::Enum::Type  type,
       const vector<string>       &basepaths,
       const char                 *fusepath,
       const uint64_t              minfreespace,
       vector<const string*>      &paths)
{
  if(type == Category::Enum::create)
    return _eplus_create(basepaths,fusepath,minfreespace,paths);

  return _eplus_other(basepaths,fusepath,paths);
}

namespace mergerfs
{
  int
  Policy::Func::eplus(const Category::Enum::Type  type,
                      const vector<string>       &basepaths,
                      const char                 *fusepath,
                      const uint64_t              minfreespace,
                      vector<const string*>      &paths)
  {
    int rv;

    rv = _eplus(type,basepaths,fusepath,minfreespace,paths);
    if(rv == -1)
      rv = Policy::Func::lus(type,basepaths,fusepath,minfreespace,paths);

    return rv;
  }
}
