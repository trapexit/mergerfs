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
#include "fs_link.hpp"
#include "fs_lstat.hpp"
#include "fs_path.hpp"
#include "fuse_getattr.hpp"
#include "fuse_symlink.hpp"
#include "ghc/filesystem.hpp"
#include "state.hpp"
#include "ugid.hpp"

#include "fuse.h"


namespace gfs = ghc::filesystem;


namespace l
{
  static
  int
  link_exdev_rel_symlink(const char      *oldpath_,
                         const char      *newpath_,
                         struct stat     *st_,
                         fuse_timeouts_t *timeouts_)
  {
    int rv;
    gfs::path target(oldpath_);
    gfs::path linkpath(newpath_);

    target = target.lexically_relative(linkpath.parent_path());

    rv = FUSE::SYMLINK::symlink(target.c_str(),linkpath.c_str());
    if(rv == 0)
      rv = FUSE::GETATTR::getattr(oldpath_,st_,timeouts_);

    // Disable caching since we created a symlink but should be a regular.
    timeouts_->attr  = 0;
    timeouts_->entry = 0;

    return rv;
  }

  static
  int
  link_exdev_abs_base_symlink(const char      *oldpath_,
                              const char      *newpath_,
                              struct stat     *st_,
                              fuse_timeouts_t *timeouts_)
  {
    int rv;
    StrVec basepaths;
    gfs::path target;

    target = "FIXME";

    rv = FUSE::SYMLINK::symlink(target.c_str(),newpath_);
    if(rv == 0)
      rv = FUSE::GETATTR::getattr(oldpath_,st_,timeouts_);

    // Disable caching since we created a symlink but should be a regular.
    timeouts_->attr  = 0;
    timeouts_->entry = 0;

    return rv;
  }

  static
  int
  link_exdev_abs_pool_symlink(const gfs::path &mount_,
                              const char      *oldpath_,
                              const char      *newpath_,
                              struct stat     *st_,
                              fuse_timeouts_t *timeouts_)
  {
    int rv;
    gfs::path target;

    target = mount_ / &oldpath_[1];

    rv = FUSE::SYMLINK::symlink(target.c_str(),newpath_);
    if(rv == 0)
      rv = FUSE::GETATTR::getattr(oldpath_,st_,timeouts_);

    // Disable caching since we created a symlink but should be a regular.
    timeouts_->attr  = 0;
    timeouts_->entry = 0;

    return rv;
  }

  static
  int
  link_exdev(State           &s_,
             const char      *oldpath_,
             const char      *newpath_,
             struct stat     *st_,
             fuse_timeouts_t *timeouts_)
  {
    switch(s_->link_exdev)
      {
      case LinkEXDEV::INVALID:
      case LinkEXDEV::PASSTHROUGH:
        return -EXDEV;
      case LinkEXDEV::REL_SYMLINK:
        return l::link_exdev_rel_symlink(oldpath_,
                                         newpath_,
                                         st_,
                                         timeouts_);
      case LinkEXDEV::ABS_BASE_SYMLINK:
        return l::link_exdev_abs_base_symlink(oldpath_,
                                              newpath_,
                                              st_,
                                              timeouts_);
      case LinkEXDEV::ABS_POOL_SYMLINK:
        return l::link_exdev_abs_pool_symlink(s_->mountpoint,
                                              oldpath_,
                                              newpath_,
                                              st_,
                                              timeouts_);
      }

    return -EXDEV;
  }
}

namespace FUSE::LINK
{
  int
  link(const char      *oldpath_,
       const char      *newpath_,
       struct stat     *st_,
       fuse_timeouts_t *timeouts_)
  {
    int rv;
    State s;
    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    rv = s->link(oldpath_,newpath_);
    if(rv == -EXDEV)
      return l::link_exdev(s,oldpath_,newpath_,st_,timeouts_);

    return s->getattr(newpath_,st_,timeouts_);
  }
}
