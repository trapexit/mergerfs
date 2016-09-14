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

#include <fuse.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "config.hpp"
#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "rv.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using mergerfs::Policy;
using namespace mergerfs;

static
int
_mkdir_loop_core(const string &existingpath,
                 const string &createpath,
                 const char   *fusepath,
                 const char   *fusedirpath,
                 const mode_t  mode,
                 const int     error)
{
  int rv;
  string fullpath;

  if(createpath != existingpath)
    {
      const ugid::SetRootGuard ugidGuard;
      fs::clonepath(existingpath,createpath,fusedirpath);
    }

  fs::path::make(&createpath,fusepath,fullpath);

  rv = ::mkdir(fullpath.c_str(),mode);

  return calc_error(rv,error,errno);
}

static
int
_mkdir_loop(const string                &existingpath,
            const vector<const string*> &createpaths,
            const char                  *fusepath,
            const char                  *fusedirpath,
            const mode_t                 mode)
{
  int error;

  error = -1;
  for(size_t i = 0, ei = createpaths.size(); i != ei; i++)
    {
      error = _mkdir_loop_core(existingpath,*createpaths[i],
                               fusepath,fusedirpath,mode,error);
    }

  return -error;
}

static
int
_mkdir(Policy::Func::Search  searchFunc,
       Policy::Func::Create  createFunc,
       const vector<string> &srcmounts,
       const uint64_t        minfreespace,
       const char           *fusepath,
       const mode_t          mode)
{
  int rv;
  string fusedirpath;
  const char *fusedirpathcstr;
  vector<const string*> createpaths;
  vector<const string*> existingpaths;

  fusedirpath = fusepath;
  fs::path::dirname(fusedirpath);
  fusedirpathcstr = fusedirpath.c_str();

  rv = searchFunc(srcmounts,fusedirpathcstr,minfreespace,existingpaths);
  if(rv == -1)
    return -errno;

  rv = createFunc(srcmounts,fusedirpathcstr,minfreespace,createpaths);
  if(rv == -1)
    return -errno;

  return _mkdir_loop(*existingpaths[0],createpaths,
                     fusepath,fusedirpathcstr,mode);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    mkdir(const char *fusepath,
          mode_t      mode)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.srcmountslock);

      return _mkdir(config.getattr,
                    config.mkdir,
                    config.srcmounts,
                    config.minfreespace,
                    fusepath,
                    (mode & ~fc->umask));
    }
  }
}
