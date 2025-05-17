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
#include "fs_lchmod.hpp"
#include "fs_path.hpp"
#include "policy_rv.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>

#include <string.h>


namespace l
{
  static
  void
  chmod_loop_core(const std::string &basepath_,
                  const char        *fusepath_,
                  const mode_t       mode_,
                  PolicyRV          *prv_)
  {
    std::string fullpath;

    fullpath = fs::path::make(basepath_,fusepath_);

    errno = 0;
    fs::lchmod(fullpath,mode_);

    prv_->insert(errno,basepath_);
  }

  static
  void
  chmod_loop(const std::vector<Branch*> &branches_,
             const char                 *fusepath_,
             const mode_t                mode_,
             PolicyRV                   *prv_)
  {
    for(auto &branch : branches_)
      {
        l::chmod_loop_core(branch->path,fusepath_,mode_,prv_);
      }
  }

  static
  int
  chmod(const Policy::Action &actionFunc_,
        const Policy::Search &searchFunc_,
        const Branches       &branches_,
        const char           *fusepath_,
        const mode_t          mode_)
  {
    int rv;
    PolicyRV prv;
    std::vector<Branch*> branches;

    rv = actionFunc_(branches_,fusepath_,branches);
    if(rv == -1)
      return -errno;

    l::chmod_loop(branches,fusepath_,mode_,&prv);
    if(prv.errors.empty())
      return 0;
    if(prv.successes.empty())
      return prv.errors[0].rv;

    branches.clear();
    rv = searchFunc_(branches_,fusepath_,branches);
    if(rv == -1)
      return -errno;

    return prv.get_error(branches[0]->path);
  }
}

namespace FUSE
{
  int
  chmod(const char *fusepath_,
        mode_t      mode_)
  {
    Config::Read cfg;
    const fuse_context *fc  = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::chmod(cfg->func.chmod.policy,
                    cfg->func.getattr.policy,
                    cfg->branches,
                    fusepath_,
                    mode_);
  }
}
