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
#include <sys/stat.h>

#include <string>
#include <vector>

#include "fs_path.hpp"
#include "policy.hpp"
#include "success_fail.hpp"

using std::string;
using std::vector;
using std::size_t;

static
int
_ffwp(const vector<string>  &basepaths,
      const char            *fusepath,
      vector<const string*> &paths)
{
  int rv;
  struct stat st;
  string fullpath;
  const string *fallback;

  fallback = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      const string *basepath = &basepaths[i];

      fs::path::make(basepath,fusepath,fullpath);

      rv = ::lstat(fullpath.c_str(),&st);
      if(LSTAT_SUCCEEDED(rv))
        {
          paths.push_back(basepath);
          return POLICY_SUCCESS;
        }
      else if(errno == EACCES)
        {
          fallback = basepath;
        }
    }

  if(fallback == NULL)
    return (errno=ENOENT,POLICY_FAIL);

  paths.push_back(fallback);

  return POLICY_SUCCESS;
}

namespace mergerfs
{
  int
  Policy::Func::ffwp(const Category::Enum::Type  type,
                     const vector<string>       &basepaths,
                     const char                 *fusepath,
                     const size_t                minfreespace,
                     vector<const string*>      &paths)
  {
    return _ffwp(basepaths,fusepath,paths);
  }
}
