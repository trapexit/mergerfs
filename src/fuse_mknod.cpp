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

#include "fuse_mknod.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "error.hpp"
#include "fs_acl.hpp"
#include "fs_mknod_as.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>
#include <vector>


static
inline
int
_mknod_core(const ugid_t    ugid_,
            const fs::path &fullpath_,
            mode_t          mode_,
            const mode_t    umask_,
            const dev_t     dev_)
{
  if(!fs::acl::dir_has_defaults(fullpath_))
    mode_ &= ~umask_;

  return fs::mknod_as(ugid_,fullpath_,mode_,dev_);
}

static
int
_mknod_loop_core(const ugid_t    ugid_,
                 const fs::path &createbranch_,
                 const fs::path &fusepath_,
                 const mode_t    mode_,
                 const mode_t    umask_,
                 const dev_t     dev_)
{
  int rv;
  fs::path fullpath;

  fullpath = createbranch_ / fusepath_;

  rv = ::_mknod_core(ugid_,fullpath,mode_,umask_,dev_);

  return rv;
}

static
int
_mknod_loop(const ugid_t                ugid_,
            const fs::path             &existingbranch_,
            const std::vector<Branch*> &createbranches_,
            const fs::path             &fusepath_,
            const fs::path             &fusedirpath_,
            const mode_t                mode_,
            const mode_t                umask_,
            const dev_t                 dev_)
{
  int rv;
  Err err;

  for(const auto &createbranch : createbranches_)
    {
      rv = fs::clonepath(existingbranch_,
                         createbranch->path,
                         fusedirpath_);
      if(rv < 0)
        {
          err = rv;
          continue;
        }

      err = ::_mknod_loop_core(ugid_,
                               createbranch->path,
                               fusepath_,
                               mode_,
                               umask_,
                               dev_);
    }

  return err;
}

static
int
_mknod(const ugid_t          ugid_,
       const Policy::Search &searchFunc_,
       const Policy::Create &createFunc_,
       const Branches       &branches_,
       const fs::path       &fusepath_,
       const mode_t          mode_,
       const mode_t          umask_,
       const dev_t           dev_)
{
  int rv;
  fs::path fusedirpath;
  std::vector<Branch*> createbranches;
  std::vector<Branch*> existingbranches;

  fusedirpath = fusepath_.parent_path();

  rv = searchFunc_(branches_,fusedirpath,existingbranches);
  if(rv < 0)
    return rv;

  rv = createFunc_(branches_,fusedirpath,createbranches);
  if(rv < 0)
    return rv;

  return ::_mknod_loop(ugid_,
                       existingbranches[0]->path,
                       createbranches,
                       fusepath_,
                       fusedirpath,
                       mode_,
                       umask_,
                       dev_);
}

int
FUSE::mknod(const fuse_req_ctx_t *ctx_,
            const char           *fusepath_,
            mode_t                mode_,
            dev_t                 rdev_)
{
  int rv;
  const fs::path fusepath{fusepath_};

  rv = ::_mknod(ctx_,
                cfg.func.getattr.policy,
                cfg.func.mknod.policy,
                cfg.branches,
                fusepath,
                mode_,
                ctx_->umask,
                rdev_);
  if(rv == -EROFS)
    {
      cfg.branches.find_and_set_mode_ro();
      rv = ::_mknod(ctx_,
                    cfg.func.getattr.policy,
                    cfg.func.mknod.policy,
                    cfg.branches,
                    fusepath,
                    mode_,
                    ctx_->umask,
                    rdev_);
    }

  return rv;
}
