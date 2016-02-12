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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/statvfs.h>

#include <string>
#include <vector>

#include "fs_path.hpp"
#include "policy.hpp"

using std::string;
using std::vector;
using std::size_t;
using mergerfs::Policy;
using mergerfs::Category;
typedef struct statvfs statvfs_t;

static
void
_calc_mfs(const statvfs_t  &fsstats,
          const string     *basepath,
          const size_t      minfreespace,
          fsblkcnt_t       &mfs,
          const string    *&mfsbasepath)
{
  fsblkcnt_t spaceavail;

  spaceavail = (fsstats.f_frsize * fsstats.f_bavail);
  if((spaceavail > minfreespace) && (spaceavail > mfs))
    {
      mfs         = spaceavail;
      mfsbasepath = basepath;
    }
}

static
int
_epmfs_create(const vector<string>  &basepaths,
              const char            *fusepath,
              const size_t           minfreespace,
              vector<const string*> &paths)
{
  int rv;
  string fullpath;
  statvfs_t fsstats;
  fsblkcnt_t epmfs;
  const string *epmfsbasepath;

  epmfs = 0;
  epmfsbasepath = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      const string *basepath = &basepaths[i];

      fs::path::make(basepath,fusepath,fullpath);

      rv = ::statvfs(fullpath.c_str(),&fsstats);
      if(rv == 0)
        _calc_mfs(fsstats,basepath,minfreespace,epmfs,epmfsbasepath);
    }

  if(epmfsbasepath == NULL)
    return Policy::Func::mfs(Category::Enum::create,basepaths,fusepath,minfreespace,paths);

  paths.push_back(epmfsbasepath);

  return 0;
}

static
int
_epmfs(const vector<string>  &basepaths,
       const char            *fusepath,
       vector<const string*> &paths)

{
  int rv;
  string fullpath;
  statvfs_t fsstats;
  fsblkcnt_t epmfs;
  const string *epmfsbasepath;

  epmfs = 0;
  epmfsbasepath = NULL;
  for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
    {
      const string *basepath = &basepaths[i];

      fs::path::make(basepath,fusepath,fullpath);

      rv = ::statvfs(fullpath.c_str(),&fsstats);
      if(rv == 0)
        _calc_mfs(fsstats,basepath,0,epmfs,epmfsbasepath);
    }

  if(epmfsbasepath == NULL)
    return (errno=ENOENT,-1);

  paths.push_back(epmfsbasepath);

  return 0;
}

namespace mergerfs
{
  int
  Policy::Func::epmfs(const Category::Enum::Type  type,
                      const vector<string>       &basepaths,
                      const char                 *fusepath,
                      const size_t                minfreespace,
                      vector<const string*>     &paths)
  {
    if(type == Category::Enum::create)
      return _epmfs_create(basepaths,fusepath,minfreespace,paths);

    return _epmfs(basepaths,fusepath,paths);
  }
}
