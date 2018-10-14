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

static
int
_ff_create(const vector<string>  &basepaths,
           const uint64_t         minfreespace,
           vector<const string*> &paths)
{
  int rv;
  fs::info_t info;
  const string *basepath;
  const string *fallback;

  fallback = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      basepath = &basepaths[i];

      rv = fs::info(basepath,&info);
      if(rv == -1)
        continue;
      if(info.readonly)
        continue;
      if(fallback == NULL)
        fallback = basepath;
      if(info.spaceavail < minfreespace)
        continue;

      paths.push_back(basepath);

      return 0;
    }

  if(fallback == NULL)
    return (errno=ENOENT,-1);

  paths.push_back(fallback);

  return 0;
}

static
int
_ff_other(const vector<string>  &basepaths,
          const char            *fusepath,
          vector<const string*> &paths)
{
  const string *basepath;

  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      basepath = &basepaths[i];

      if(!fs::exists(*basepath,fusepath))
        continue;

      paths.push_back(basepath);

      return 0;
    }

  return (errno=ENOENT,-1);
}

namespace mergerfs
{
  int
  Policy::Func::ff(const Category::Enum::Type  type,
                   const vector<string>       &basepaths,
                   const char                 *fusepath,
                   const uint64_t              minfreespace,
                   vector<const string*>      &paths)
  {
    if(type == Category::Enum::create)
      return _ff_create(basepaths,minfreespace,paths);

    return _ff_other(basepaths,fusepath,paths);
  }
}
