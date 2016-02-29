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

#include <errno.h>

#include <string>
#include <vector>

#include "fs.hpp"
#include "fs_path.hpp"
#include "policy.hpp"

using std::string;
using std::vector;
using std::size_t;

static
int
_all_create(const vector<string>  &srcmounts,
            const size_t           minfreespace,
            vector<const string*> &paths)
{
  for(size_t i = 0, ei = srcmounts.size(); i != ei; i++)
    {
      const string *basepath = &srcmounts[i];

      if(!fs::exists_on_rw_fs_with_at_least(*basepath,minfreespace))
        continue;

      paths.push_back(basepath);
    }

  if(paths.empty())
    return POLICY_FAIL_ENOENT;

  return POLICY_SUCCESS;
}

static
int
_all(const vector<string>  &srcmounts,
     const char            *fusepath,
     vector<const string*> &paths)
{
  string fullpath;

  for(size_t i = 0, ei = srcmounts.size(); i != ei; i++)
    {
      const string *basepath = &srcmounts[i];

      fs::path::make(basepath,fusepath,fullpath);

      if(!fs::exists(fullpath))
        continue;

      paths.push_back(basepath);
    }

  if(paths.empty())
    return POLICY_FAIL_ENOENT;

  return POLICY_SUCCESS;
}

namespace mergerfs
{
  int
  Policy::Func::all(const Category::Enum::Type  type,
                    const vector<string>       &srcmounts,
                    const char                 *fusepath,
                    const size_t                minfreespace,
                    vector<const string*>      &paths)
  {
    if(type == Category::Enum::create)
      return _all_create(srcmounts,minfreespace,paths);

    return _all(srcmounts,fusepath,paths);
  }
}
