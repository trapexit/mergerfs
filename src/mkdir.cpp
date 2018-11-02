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

#include <string>
#include <vector>

#include "config.hpp"
#include "errno.hpp"
#include "fs_acl.hpp"
#include "fs_base_mkdir.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "rv.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using namespace mergerfs;

static
int
_mkdir_core(const string &fullpath,
            mode_t        mode,
            const mode_t  umask)
{
  if(!fs::acl::dir_has_defaults(fullpath))
    mode &= ~umask;

  return fs::mkdir(fullpath,mode);
}

static
int
_mkdir_loop_core(const string &createpath,
                 const char   *fusepath,
                 const mode_t  mode,
                 const mode_t  umask,
                 const int     error)
{
  int rv;
  string fullpath;

  fs::path::make(&createpath,fusepath,fullpath);

  rv = _mkdir_core(fullpath,mode,umask);

  return error::calc(rv,error,errno);
}

static
int
_mkdir_loop(const string                &existingpath,
            const vector<const string*> &createpaths,
            const char                  *fusepath,
            const string                &fusedirpath,
            const mode_t                 mode,
            const mode_t                 umask)
{
  int rv;
  int error;

  error = -1;
  for(size_t i = 0, ei = createpaths.size(); i != ei; i++)
    {
      rv = fs::clonepath_as_root(existingpath,*createpaths[i],fusedirpath);
      if(rv == -1)
        error = error::calc(rv,error,errno);
      else
        error = _mkdir_loop_core(*createpaths[i],
                                 fusepath,
                                 mode,umask,error);
    }

  return -error;
}

static
int
_mkdir(Policy::Func::Search  searchFunc,
       Policy::Func::Create  createFunc,
       const Branches       &branches_,
       const uint64_t        minfreespace,
       const char           *fusepath,
       const mode_t          mode,
       const mode_t          umask)
{
  int rv;
  string fusedirpath;
  vector<const string*> createpaths;
  vector<const string*> existingpaths;

  fusedirpath = fs::path::dirname(fusepath);

  rv = searchFunc(branches_,fusedirpath,minfreespace,existingpaths);
  if(rv == -1)
    return -errno;

  rv = createFunc(branches_,fusedirpath,minfreespace,createpaths);
  if(rv == -1)
    return -errno;

  return _mkdir_loop(*existingpaths[0],createpaths,
                     fusepath,fusedirpath,mode,umask);
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
      const rwlock::ReadGuard  readlock(&config.branches_lock);

      return _mkdir(config.getattr,
                    config.mkdir,
                    config.branches,
                    config.minfreespace,
                    fusepath,
                    mode,
                    fc->umask);
    }
  }
}
