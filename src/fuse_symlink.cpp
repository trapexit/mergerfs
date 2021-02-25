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
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "fs_symlink.hpp"
#include "fuse_getattr.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>

#include <sys/types.h>
#include <unistd.h>

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
  symlink_loop_core(const string &newbasepath_,
                    const char   *target_,
                    const char   *linkpath_,
                    const int     error_)
  {
    int rv;
    string fullnewpath;

    fullnewpath = fs::path::make(newbasepath_,linkpath_);

    rv = fs::symlink(target_,fullnewpath);

    return error::calc(rv,error_,errno);
  }

  static
  int
  symlink_loop(const string &existingpath_,
               const StrVec &newbasepaths_,
               const char   *target_,
               const char   *linkpath_,
               const string &newdirpath_)
  {
    int rv;
    int error;

    error = -1;
    for(size_t i = 0, ei = newbasepaths_.size(); i != ei; i++)
      {
        rv = fs::clonepath_as_root(existingpath_,newbasepaths_[i],newdirpath_);
        if(rv == -1)
          error = error::calc(rv,error,errno);
        else
          error = l::symlink_loop_core(newbasepaths_[i],
                                       target_,
                                       linkpath_,
                                       error);
      }

    return -error;
  }

  static
  int
  symlink(const Policy::Search &searchFunc_,
          const Policy::Create &createFunc_,
          const Branches       &branches_,
          const char           *target_,
          const char           *linkpath_)
  {
    int rv;
    string newdirpath;
    StrVec newbasepaths;
    StrVec existingpaths;

    newdirpath = fs::path::dirname(linkpath_);

    rv = searchFunc_(branches_,newdirpath,&existingpaths);
    if(rv == -1)
      return -errno;

    rv = createFunc_(branches_,newdirpath,&newbasepaths);
    if(rv == -1)
      return -errno;

    return l::symlink_loop(existingpaths[0],newbasepaths,
                           target_,linkpath_,newdirpath);
  }
}

namespace FUSE
{
  int
  symlink(const char *target_,
          const char *linkpath_)
  {
    Config::Read cfg;
    const fuse_context *fc  = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::symlink(cfg->func.getattr.policy,
                      cfg->func.symlink.policy,
                      cfg->branches,
                      target_,
                      linkpath_);
  }

  int
  symlink(const char      *target_,
          const char      *linkpath_,
          struct stat     *st_,
          fuse_timeouts_t *timeout_)
  {
    int rv;

    rv = FUSE::symlink(target_,linkpath_);
    if(rv < 0)
      return rv;

    return FUSE::getattr(target_,st_,timeout_);
  }
}
