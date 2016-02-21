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
void
_calc_mfs(const struct statvfs  &st,
          const string          *basepath,
          fsblkcnt_t            &mfs,
          const string         *&mfsbasepath)
{
  fsblkcnt_t spaceavail;

  spaceavail = StatVFS::spaceavail(st);
  if(spaceavail > mfs)
    {
      mfs         = spaceavail;
      mfsbasepath = basepath;
    }
}

static
int
_mfs(const vector<string>  &basepaths,
     const char            *fusepath,
     const bool             needswritablefs,
     vector<const string*> &paths)
{
  string fullpath;
  struct statvfs st;
  fsblkcnt_t mfs;
  const string *mfsbasepath;

  mfs = 0;
  mfsbasepath = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      const string *basepath = &basepaths[i];

      fs::path::make(basepath,fusepath,fullpath);

      if(!fs::available(fullpath,needswritablefs,st))
        continue;

      _calc_mfs(st,basepath,mfs,mfsbasepath);
    }

  if(mfsbasepath == NULL)
    return (errno=ENOENT,POLICY_FAIL);

  paths.push_back(mfsbasepath);

  return POLICY_SUCCESS;
}

namespace mergerfs
{
  int
  Policy::Func::mfs(const Category::Enum::Type  type,
                    const vector<string>       &basepaths,
                    const char                 *fusepath,
                    const size_t                minfreespace,
                    vector<const string*>      &paths)
  {
    int rv;
    const char *fp =
      ((type == Category::Enum::create) ? "" : fusepath);
    const bool needswritablefs =
      (type == Category::Enum::create);

    rv = _mfs(basepaths,fp,needswritablefs,paths);
    if(POLICY_FAILED(rv))
      rv = Policy::Func::ff(type,basepaths,fusepath,minfreespace,paths);

    return rv;
  }
}
