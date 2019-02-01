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
#include "fs_base_symlink.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "rv.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

#include <fuse.h>

#include <string>

#include <sys/types.h>
#include <unistd.h>

using std::string;
using std::vector;

namespace l
{
  static
  int
  symlink_loop_core(const string &newbasepath_,
                    const char   *oldpath_,
                    const char   *newpath_,
                    const int     error_)
  {
    int rv;
    string fullnewpath;

    fullnewpath = fs::path::make(&newbasepath_,newpath_);

    rv = fs::symlink(oldpath_,fullnewpath);

    return error::calc(rv,error_,errno);
  }

  static
  int
  symlink_loop(const string                &existingpath_,
               const vector<const string*>  newbasepaths_,
               const char                  *oldpath_,
               const char                  *newpath_,
               const string                &newdirpath_)
  {
    int rv;
    int error;

    error = -1;
    for(size_t i = 0, ei = newbasepaths_.size(); i != ei; i++)
      {
        rv = fs::clonepath_as_root(existingpath_,*newbasepaths_[i],newdirpath_);
        if(rv == -1)
          error = error::calc(rv,error,errno);
        else
          error = l::symlink_loop_core(*newbasepaths_[i],
                                       oldpath_,
                                       newpath_,
                                       error);
      }

    return -error;
  }

  static
  int
  symlink(Policy::Func::Search  searchFunc_,
          Policy::Func::Create  createFunc_,
          const Branches       &branches_,
          const uint64_t        minfreespace_,
          const char           *oldpath_,
          const char           *newpath_)
  {
    int rv;
    string newdirpath;
    vector<const string*> newbasepaths;
    vector<const string*> existingpaths;

    newdirpath = fs::path::dirname(newpath_);

    rv = searchFunc_(branches_,newdirpath,minfreespace_,existingpaths);
    if(rv == -1)
      return -errno;

    rv = createFunc_(branches_,newdirpath,minfreespace_,newbasepaths);
    if(rv == -1)
      return -errno;

    return l::symlink_loop(*existingpaths[0],newbasepaths,
                           oldpath_,newpath_,newdirpath);
  }
}

namespace FUSE
{
  int
  symlink(const char *oldpath_,
          const char *newpath_)
  {
    const fuse_context      *fc     = fuse_get_context();
    const Config            &config = Config::get(fc);
    const ugid::Set          ugid(fc->uid,fc->gid);
    const rwlock::ReadGuard  readlock(&config.branches_lock);

    return l::symlink(config.getattr,
                      config.symlink,
                      config.branches,
                      config.minfreespace,
                      oldpath_,
                      newpath_);
  }
}
