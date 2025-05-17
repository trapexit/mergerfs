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
#include "fs_lremovexattr.hpp"
#include "fs_path.hpp"
#include "policy_rv.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>
#include <vector>

using std::string;
using std::vector;


namespace l
{
  static
  void
  removexattr_loop_core(const string &basepath_,
                        const char   *fusepath_,
                        const char   *attrname_,
                        PolicyRV     *prv_)
  {
    string fullpath;

    fullpath = fs::path::make(basepath_,fusepath_);

    errno = 0;
    fs::lremovexattr(fullpath,attrname_);

    prv_->insert(errno,basepath_);
  }

  static
  void
  removexattr_loop(const std::vector<Branch*> &branches_,
                   const char                 *fusepath_,
                   const char                 *attrname_,
                   PolicyRV                   *prv_)
  {
    for(auto &branch : branches_)
      {
        l::removexattr_loop_core(branch->path,fusepath_,attrname_,prv_);
      }
  }

  static
  int
  removexattr(const Policy::Action &actionFunc_,
              const Policy::Search &searchFunc_,
              const Branches       &ibranches_,
              const char           *fusepath_,
              const char           *attrname_)
  {
    int rv;
    PolicyRV prv;
    std::vector<Branch*> obranches;

    rv = actionFunc_(ibranches_,fusepath_,obranches);
    if(rv == -1)
      return -errno;

    l::removexattr_loop(obranches,fusepath_,attrname_,&prv);
    if(prv.errors.empty())
      return 0;
    if(prv.successes.empty())
      return prv.errors[0].rv;

    obranches.clear();
    rv = searchFunc_(ibranches_,fusepath_,obranches);
    if(rv == -1)
      return -errno;

    return prv.get_error(obranches[0]->path);
  }
}

namespace FUSE
{
  int
  removexattr(const char *fusepath_,
              const char *attrname_)
  {
    Config::Read cfg;

    if(fusepath_ == CONTROLFILE)
      return -ENOATTR;

    if(cfg->xattr.to_int())
      return -cfg->xattr.to_int();

    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::removexattr(cfg->func.removexattr.policy,
                          cfg->func.getxattr.policy,
                          cfg->branches,
                          fusepath_,
                          attrname_);
  }
}
