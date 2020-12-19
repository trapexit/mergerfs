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
#include "fs_clonepath.hpp"
#include "fs_mkdir.hpp"
#include "fs_path.hpp"
#include "policy.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>

using std::string;


namespace error
{
  static
  inline
  int
  calc(const int rv_,
       const int prev_,
       const int cur_)
  {
    if(rv_ == -1)
      {
        if(prev_ == 0)
          return 0;
        return cur_;
      }

    return 0;
  }
}

namespace l
{
  static
  int
  mkdir_core(const string &fullpath_,
             mode_t        mode_,
             const mode_t  umask_)
  {
    if(!fs::acl::dir_has_defaults(fullpath_))
      mode_ &= ~umask_;

    return fs::mkdir(fullpath_,mode_);
  }

  static
  int
  mkdir_loop_core(const string &createpath_,
                  const char   *fusepath_,
                  const mode_t  mode_,
                  const mode_t  umask_,
                  const int     error_)
  {
    int rv;
    string fullpath;

    fullpath = fs::path::make(createpath_,fusepath_);

    rv = l::mkdir_core(fullpath,mode_,umask_);

    return error::calc(rv,error_,errno);
  }

  static
  int
  mkdir_loop(const string &existingpath_,
             const StrVec &createpaths_,
             const char   *fusepath_,
             const string &fusedirpath_,
             const mode_t  mode_,
             const mode_t  umask_)
  {
    int rv;
    int error;

    error = -1;
    for(size_t i = 0, ei = createpaths_.size(); i != ei; i++)
      {
        rv = fs::clonepath_as_root(existingpath_,createpaths_[i],fusedirpath_);
        if(rv == -1)
          error = error::calc(rv,error,errno);
        else
          error = l::mkdir_loop_core(createpaths_[i],
                                     fusepath_,
                                     mode_,
                                     umask_,
                                     error);
      }

    return -error;
  }

  static
  int
  mkdir(const Policy::Search &getattrPolicy_,
        const Policy::Create &mkdirPolicy_,
        const Branches       &branches_,
        const char           *fusepath_,
        const mode_t          mode_,
        const mode_t          umask_)
  {
    int rv;
    string fusedirpath;
    StrVec createpaths;
    StrVec existingpaths;

    fusedirpath = fs::path::dirname(fusepath_);

    rv = getattrPolicy_(branches_,fusedirpath.c_str(),&existingpaths);
    if(rv == -1)
      return -errno;

    rv = mkdirPolicy_(branches_,fusedirpath.c_str(),&createpaths);
    if(rv == -1)
      return -errno;

    return l::mkdir_loop(existingpaths[0],
                         createpaths,
                         fusepath_,
                         fusedirpath,
                         mode_,
                         umask_);
  }
}

namespace FUSE
{
  int
  mkdir(const char *fusepath_,
        mode_t      mode_)
  {
    Config::Read cfg;
    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::mkdir(cfg->func.getattr.policy,
                    cfg->func.mkdir.policy,
                    cfg->branches,
                    fusepath_,
                    mode_,
                    fc->umask);
  }
}
