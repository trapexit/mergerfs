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

#include "fuse_chmod.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fs_lchmod.hpp"
#include "fs_path.hpp"
#include "policy_rv.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <cstring>
#include <string>


static
void
_chmod_loop_core(const std::string &basepath_,
                 const fs::path    &fusepath_,
                 const mode_t       mode_,
                 PolicyRV          *prv_)
{
  fs::path fullpath;

  fullpath = basepath_ / fusepath_;

  errno = 0;
  fs::lchmod(fullpath,mode_);

  prv_->insert(errno,basepath_);
}

static
void
_chmod_loop(const std::vector<Branch*> &branches_,
            const fs::path             &fusepath_,
            const mode_t                mode_,
            PolicyRV                   *prv_)
{
  for(auto &branch : branches_)
    {
      ::_chmod_loop_core(branch->path,fusepath_,mode_,prv_);
    }
}

static
int
_chmod(const Policy::Action &actionFunc_,
       const Policy::Search &searchFunc_,
       const Branches       &branches_,
       const fs::path       &fusepath_,
       const mode_t          mode_)
{
  int rv;
  PolicyRV prv;
  std::vector<Branch*> branches;

  rv = actionFunc_(branches_,fusepath_,branches);
  if(rv < 0)
    return rv;

  ::_chmod_loop(branches,fusepath_,mode_,&prv);
  if(prv.errors.empty())
    return 0;
  if(prv.successes.empty())
    return prv.errors[0].rv;

  branches.clear();
  rv = searchFunc_(branches_,fusepath_,branches);
  if(rv < 0)
    return rv;

  return prv.get_error(branches[0]->path);
}

static
int
_chmod(const fs::path &fusepath_,
       const mode_t    mode_)
{
  const fuse_context *fc  = fuse_get_context();
  const ugid::Set     ugid(fc->uid,fc->gid);

  return ::_chmod(cfg.func.chmod.policy,
                  cfg.func.getattr.policy,
                  cfg.branches,
                  fusepath_,
                  mode_);
}

int
FUSE::chmod(const char *fusepath_,
            mode_t      mode_)
{
  const fs::path fusepath{fusepath_};

  return ::_chmod(fusepath,mode_);
}
