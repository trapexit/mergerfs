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
#include "fs_mknod.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>
#include <vector>


static
inline
int
_mknod_core(const std::string &fullpath_,
            mode_t             mode_,
            const mode_t       umask_,
            const dev_t        dev_)
{
  if(!fs::acl::dir_has_defaults(fullpath_))
    mode_ &= ~umask_;

  return fs::mknod(fullpath_,mode_,dev_);
}

static
int
_mknod_loop_core(const std::string &createbranch_,
                 const char        *fusepath_,
                 const mode_t       mode_,
                 const mode_t       umask_,
                 const dev_t        dev_)
{
  int rv;
  std::string fullpath;

  fullpath = fs::path::make(createbranch_,fusepath_);

  rv = ::_mknod_core(fullpath,mode_,umask_,dev_);

  return rv;
}

static
int
_mknod_loop(const std::string          &existingbranch_,
            const std::vector<Branch*> &createbranches_,
            const char                 *fusepath_,
            const std::string          &fusedirpath_,
            const mode_t                mode_,
            const mode_t                umask_,
            const dev_t                 dev_)
{
  int rv;
  Err err;

  for(const auto &createbranch : createbranches_)
    {
      rv = fs::clonepath_as_root(existingbranch_,
                                 createbranch->path,
                                 fusedirpath_);
      if(rv < 0)
        {
          err = rv;
          continue;
        }

      err = ::_mknod_loop_core(createbranch->path,
                               fusepath_,
                               mode_,
                               umask_,
                               dev_);
    }

  return err;
}

static
int
_mknod(const Policy::Search &searchFunc_,
       const Policy::Create &createFunc_,
       const Branches       &branches_,
       const char           *fusepath_,
       const mode_t          mode_,
       const mode_t          umask_,
       const dev_t           dev_)
{
  int rv;
  std::string fusedirpath;
  std::vector<Branch*> createbranches;
  std::vector<Branch*> existingbranches;

  fusedirpath = fs::path::dirname(fusepath_);

  rv = searchFunc_(branches_,fusedirpath,existingbranches);
  if(rv < 0)
    return rv;

  rv = createFunc_(branches_,fusedirpath,createbranches);
  if(rv < 0)
    return rv;

  return ::_mknod_loop(existingbranches[0]->path,
                       createbranches,
                       fusepath_,
                       fusedirpath,
                       mode_,
                       umask_,
                       dev_);
}

int
FUSE::mknod(const char *fusepath_,
            mode_t      mode_,
            dev_t       rdev_)
{
  int rv;
  Config::Read cfg;
  const fuse_context *fc = fuse_get_context();
  const ugid::Set     ugid(fc->uid,fc->gid);

  rv = ::_mknod(cfg->func.getattr.policy,
                cfg->func.mknod.policy,
                cfg->branches,
                fusepath_,
                mode_,
                fc->umask,
                rdev_);
  if(rv == -EROFS)
    {
      Config::Write()->branches.find_and_set_mode_ro();
      rv = ::_mknod(cfg->func.getattr.policy,
                    cfg->func.mknod.policy,
                    cfg->branches,
                    fusepath_,
                    mode_,
                    fc->umask,
                    rdev_);
    }

  return rv;
}
