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

#include "config.hpp"
#include "errno.hpp"
#include "fs_acl.hpp"
#include "fs_base_mknod.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "rv.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

#include <fuse.h>

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace l
{
  static
  inline
  int
  mknod_core(const string &fullpath_,
             mode_t        mode_,
             const mode_t  umask_,
             const dev_t   dev_)
  {
    if(!fs::acl::dir_has_defaults(fullpath_))
      mode_ &= ~umask_;

    return fs::mknod(fullpath_,mode_,dev_);
  }

  static
  int
  mknod_loop_core(const string &createpath_,
                  const char   *fusepath_,
                  const mode_t  mode_,
                  const mode_t  umask_,
                  const dev_t   dev_,
                  const int     error_)
  {
    int rv;
    string fullpath;

    fullpath = fs::path::make(&createpath_,fusepath_);

    rv = l::mknod_core(fullpath,mode_,umask_,dev_);

    return error::calc(rv,error_,errno);
  }

  static
  int
  mknod_loop(const string                &existingpath_,
             const vector<const string*> &createpaths_,
             const char                  *fusepath_,
             const string                &fusedirpath_,
             const mode_t                 mode_,
             const mode_t                 umask_,
             const dev_t                  dev_)
  {
    int rv;
    int error;

    error = -1;
    for(size_t i = 0, ei = createpaths_.size(); i != ei; i++)
      {
        rv = fs::clonepath_as_root(existingpath_,*createpaths_[i],fusedirpath_);
        if(rv == -1)
          error = error::calc(rv,error,errno);
        else
          error = l::mknod_loop_core(*createpaths_[i],
                                     fusepath_,
                                     mode_,umask_,dev_,error);
      }

    return -error;
  }

  static
  int
  mknod(Policy::Func::Search  searchFunc_,
        Policy::Func::Create  createFunc_,
        const Branches       &branches_,
        const uint64_t        minfreespace_,
        const char           *fusepath_,
        const mode_t          mode_,
        const mode_t          umask_,
        const dev_t           dev_)
  {
    int rv;
    string fusedirpath;
    vector<const string*> createpaths;
    vector<const string*> existingpaths;

    fusedirpath = fs::path::dirname(fusepath_);

    rv = searchFunc_(branches_,fusedirpath,minfreespace_,existingpaths);
    if(rv == -1)
      return -errno;

    rv = createFunc_(branches_,fusedirpath,minfreespace_,createpaths);
    if(rv == -1)
      return -errno;

    return l::mknod_loop(*existingpaths[0],createpaths,
                         fusepath_,fusedirpath,
                         mode_,umask_,dev_);
  }
}

namespace FUSE
{
  int
  mknod(const char *fusepath_,
        mode_t      mode_,
        dev_t       rdev_)
  {
    const fuse_context      *fc     = fuse_get_context();
    const Config            &config = Config::get(fc);
    const ugid::Set          ugid(fc->uid,fc->gid);
    const rwlock::ReadGuard  readlock(&config.branches_lock);

    return l::mknod(config.getattr,
                    config.mknod,
                    config.branches,
                    config.minfreespace,
                    fusepath_,
                    mode_,
                    fc->umask,
                    rdev_);
  }
}
