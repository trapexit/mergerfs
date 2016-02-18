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
#include <sys/statvfs.h>

#include <string>
#include <vector>

#include "fs.hpp"
#include "fs_path.hpp"
#include "policy.hpp"
#include "statvfs_util.hpp"

using std::string;
using std::vector;
using std::size_t;

static
int
_ff(const vector<string>  &basepaths,
    const char            *fusepath,
    const bool             needswritablefs,
    vector<const string*> &paths)
{
  string fullpath;
  struct statvfs st;
  const string *fallback = NULL;

  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      const string *basepath = &basepaths[i];

      fs::path::make(basepath,fusepath,fullpath);

      if(!fs::exists(fullpath,st))
        continue;

      if(needswritablefs && StatVFS::readonly(st))
        {
          if(fallback == NULL)
            fallback = basepath;
          continue;
        }

      paths.push_back(basepath);

      return POLICY_SUCCESS;
    }

  if(fallback != NULL)
    {
      paths.push_back(fallback);
      return POLICY_SUCCESS;
    }

  return (errno=ENOENT,POLICY_FAIL);
}

namespace mergerfs
{
  int
  Policy::Func::ff(const Category::Enum::Type  type,
                   const vector<string>       &basepaths,
                   const char                 *fusepath,
                   const size_t                minfreespace,
                   vector<const string*>      &paths)
  {
    const char *fp =
      ((type == Category::Enum::create) ? "" : fusepath);
    const bool needswritablefs =
      (type == Category::Enum::create);

    return _ff(basepaths,fp,needswritablefs,paths);
  }
}
