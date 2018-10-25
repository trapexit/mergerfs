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
#include "fs_path.hpp"
#include "policy.hpp"

#include <string>
#include <vector>
#include <limits>

#include <sys/stat.h>

using std::string;
using std::vector;

static
int
_newest_create(const vector<string>  &basepaths,
               const char            *fusepath,
               vector<const string*> &paths)
{
  int rv;
  bool readonly;
  time_t newest;
  struct stat st;
  const string *basepath;
  const string *newestbasepath;

  newest = std::numeric_limits<time_t>::min();
  newestbasepath = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      basepath = &basepaths[i];

      if(!fs::exists(*basepath,fusepath,st))
        continue;
      if(st.st_mtime < newest)
        continue;
      rv = fs::readonly(basepath,&readonly);
      if(rv == -1)
        continue;
      if(readonly)
        continue;

      newest = st.st_mtime;
      newestbasepath = basepath;
    }

  if(newestbasepath == NULL)
    return (errno=ENOENT,-1);

  paths.push_back(newestbasepath);

  return 0;
}

static
int
_newest_other(const vector<string>  &basepaths,
              const char            *fusepath,
              vector<const string*> &paths)
{
  time_t newest;
  struct stat st;
  const string *basepath;
  const string *newestbasepath;

  newest = std::numeric_limits<time_t>::min();
  newestbasepath = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      basepath = &basepaths[i];

      if(!fs::exists(*basepath,fusepath,st))
        continue;
      if(st.st_mtime < newest)
        continue;

      newest = st.st_mtime;
      newestbasepath = basepath;
    }

  if(newestbasepath == NULL)
    return (errno=ENOENT,-1);

  paths.push_back(newestbasepath);

  return 0;
}

namespace mergerfs
{
  int
  Policy::Func::newest(const Category::Enum::Type  type,
                       const vector<string>       &basepaths,
                       const char                 *fusepath,
                       const uint64_t              minfreespace,
                       vector<const string*>      &paths)
  {
    if(type == Category::Enum::create)
      return _newest_create(basepaths,fusepath,paths);

    return _newest_other(basepaths,fusepath,paths);
  }
}
