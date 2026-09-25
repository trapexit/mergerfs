/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_chown.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fs_lchown.hpp"
#include "fs_path.hpp"
#include "policy_rv.hpp"

#include "fuse.h"

#include <string>
#include <vector>


static
void
_chown_loop(const std::vector<Branch*> &branches_,
            const fs::relpath             &fusepath_,
            const uid_t                 uid_,
            const gid_t                 gid_,
            PolicyRV                   *prv_)
{
  fs::relpath fullpath = fusepath_;

  for(const auto &branch : branches_)
    {
      int rv;

      fullpath.set_prefix(branch->path);

      rv = fs::lchown(fullpath,uid_,gid_);

      prv_->insert(rv,branch->path);
    }
}

static
int
_chown(const Policy::Action &actionFunc_,
       const Policy::Search &searchFunc_,
       const Branches::Ptr   branches_,
       const fs::relpath       &fusepath_,
       const uid_t           uid_,
       const gid_t           gid_)
{
  int rv;
  PolicyRV prv;
  std::vector<Branch*> branches;

  rv = actionFunc_(branches_,fusepath_,branches);
  if(rv < 0)
    return rv;
  if(branches.empty())
    return -ENOENT;

  ::_chown_loop(branches,fusepath_,uid_,gid_,&prv);

  if(prv.errors.empty())
    return 0;
  if(prv.successes.empty())
    return prv.errors[0].rv;

  branches.clear();
  rv = searchFunc_(branches_,fusepath_,branches);
  if(rv < 0)
    return rv;
  if(branches.empty())
    return -ENOENT;

  return prv.get_error(branches[0]->path);
}

int
FUSE::chown(const fuse_req_ctx_t *ctx_,
            const fs::relpath       &fusepath_,
            uid_t                 uid_,
            gid_t                 gid_)
{

  return ::_chown(cfg.func.chown.policy,
                  cfg.func.getattr.policy,
                  cfg.branches,
                  fusepath_,
                  uid_,
                  gid_);
}
