/*
  Copyright (c) 2022, Antonio SJ Musumeci <trapexit@spawn.link>

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
#include "fs_link.hpp"
#include "fs_mkdir_as_root.hpp"
#include "fs_path.hpp"
#include "fs_remove.hpp"
#include "fs_rename.hpp"
#include "fs_symlink.hpp"
#include "fs_unlink.hpp"
#include "fuse_symlink.hpp"
#include "state.hpp"
#include "ugid.hpp"

#include "fs_path.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <iostream>

using std::string;

namespace l
{
  void
  rename_exdev_rename_back(const StrVec    &basepaths_,
                           const gfs::path &oldfusepath_)
  {
    gfs::path oldpath;
    gfs::path newpath;

    for(auto &basepath : basepaths_)
      {
        oldpath  = basepath;
        oldpath /= ".mergerfs_rename_exdev";
        oldpath += oldfusepath_;

        newpath  = basepath;
        newpath += oldfusepath_;

        fs::rename(oldpath,newpath);
      }
  }

  static
  int
  rename_exdev_rename_target(const gfs::path &oldfusepath_,
                             StrVec          *basepaths_)
  {
    int rv;
    gfs::path clonesrc;
    gfs::path clonetgt;

    // rv = actionPolicy_(branches_,oldfusepath_,basepaths_);
    // if(rv == -1)
    //   return -errno;

    ugid::SetRootGuard ugidGuard;
    for(auto &basepath : *basepaths_)
      {
        clonesrc  = basepath;
        clonetgt  = basepath;
        clonetgt /= ".mergerfs_rename_exdev";

        rv = fs::clonepath(clonesrc,clonetgt,oldfusepath_.parent_path());
        if((rv == -1) && (errno == ENOENT))
          {
            fs::mkdir(clonetgt,01777);
            rv = fs::clonepath(clonesrc,clonetgt,oldfusepath_.parent_path());
          }

        if(rv == -1)
          goto error;

        clonesrc += oldfusepath_;
        clonetgt += oldfusepath_;

        rv = fs::rename(clonesrc,clonetgt);
        if(rv == -1)
          goto error;
      }

    return 0;

  error:
    l::rename_exdev_rename_back(*basepaths_,oldfusepath_);

    return -EXDEV;
  }

  static
  int
  rename_exdev_rel_symlink(const Policy::Action &actionPolicy_,
                           const Branches::CPtr &branches_,
                           const gfs::path      &oldfusepath_,
                           const gfs::path      &newfusepath_)
  {
    int rv;
    StrVec basepaths;
    gfs::path target;
    gfs::path linkpath;

    // rv = l::rename_exdev_rename_target(actionPolicy_,branches_,oldfusepath_,&basepaths);
    // if(rv < 0)
    //   return rv;

    linkpath  = newfusepath_;
    target    = "/.mergerfs_rename_exdev";
    target   += oldfusepath_;
    target    = target.lexically_relative(linkpath.parent_path());

    rv = FUSE::SYMLINK::symlink(target.c_str(),linkpath.c_str());
    if(rv < 0)
      l::rename_exdev_rename_back(basepaths,oldfusepath_);

    return rv;
  }

  static
  int
  rename_exdev_abs_symlink(const gfs::path &mountpoint_,
                           const gfs::path &oldfusepath_,
                           const gfs::path &newfusepath_)
  {
    int rv;
    StrVec basepaths;
    gfs::path target;
    gfs::path linkpath;

    // rv = l::rename_exdev_rename_target(actionPolicy_,branches_,oldfusepath_,&basepaths);
    // if(rv < 0)
    //   return rv;

    linkpath  = newfusepath_;
    target    = mountpoint_;
    target   /= ".mergerfs_rename_exdev";
    target   += oldfusepath_;

    rv = FUSE::SYMLINK::symlink(target.c_str(),linkpath.c_str());
    if(rv < 0)
      l::rename_exdev_rename_back(basepaths,oldfusepath_);

    return rv;
  }

  static
  int
  rename_exdev(State           &s_,
               const gfs::path &oldfusepath_,
               const gfs::path &newfusepath_)
  {
    switch(s_->rename_exdev)
      {
      case RenameEXDEV::INVALID:
      case RenameEXDEV::PASSTHROUGH:
        return -EXDEV;
      case RenameEXDEV::REL_SYMLINK:
        return -EXDEV;
        //        return l::rename_exdev_rel_symlink(oldfusepath_,newfusepath_);
      case RenameEXDEV::ABS_SYMLINK:
        return l::rename_exdev_abs_symlink(s_->mountpoint,oldfusepath_,newfusepath_);
      }

    return -EXDEV;
  }
}

namespace FUSE::RENAME
{
  int
  rename(const char *oldfusepath_,
         const char *newfusepath_)
  {
    int rv;
    State s;
    gfs::path oldfusepath(&oldfusepath_[1]);
    gfs::path newfusepath(&newfusepath_[1]);
    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    rv = s->rename(oldfusepath_,newfusepath_);
    if(rv == -EXDEV)
      return l::rename_exdev(s,oldfusepath,newfusepath);

    return rv;
  }
}
