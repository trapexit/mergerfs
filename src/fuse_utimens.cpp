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

#include "fuse_utimens.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fs_lutimens.hpp"
#include "fs_path.hpp"
#include "policy_rv.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <fcntl.h>


static
void
_utimens_loop_core(const fs::path &basepath_,
                   const fs::path &fusepath_,
                   const timespec  ts_[2],
                   PolicyRV       *prv_)
{
  int rv;
  fs::path fullpath;

  fullpath = basepath_ / fusepath_;

  rv = fs::lutimens(fullpath,ts_);

  prv_->insert(rv,basepath_);
}

static
void
_utimens_loop(const std::vector<Branch*> &branches_,
              const fs::path             &fusepath_,
              const timespec              ts_[2],
              PolicyRV                   *prv_)
{
  for(auto &branch : branches_)
    {
      ::_utimens_loop_core(branch->path,fusepath_,ts_,prv_);
    }
}

static
int
_utimens(const Policy::Action &utimensPolicy_,
         const Policy::Search &getattrPolicy_,
         const Branches       &branches_,
         const fs::path       &fusepath_,
         const timespec        ts_[2])
{
  int rv;
  PolicyRV prv;
  std::vector<Branch*> branches;

  rv = utimensPolicy_(branches_,fusepath_,branches);
  if(rv < 0)
    return rv;

  ::_utimens_loop(branches,fusepath_,ts_,&prv);
  if(prv.errors.empty())
    return 0;
  if(prv.successes.empty())
    return prv.errors[0].rv;

  branches.clear();
  rv = getattrPolicy_(branches_,fusepath_,branches);
  if(rv < 0)
    return rv;

  return prv.get_error(branches[0]->path);
}

int
FUSE::utimens(const fuse_req_ctx_t *ctx_,
              const char           *fusepath_,
              const timespec        ts_[2])
{
  const fs::path  fusepath{fusepath_};
  const ugid::Set ugid(ctx_);

  return ::_utimens(cfg.func.utimens.policy,
                    cfg.func.getattr.policy,
                    cfg.branches,
                    fusepath,
                    ts_);
}
