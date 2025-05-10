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
#include "fs_lutimens.hpp"
#include "fs_path.hpp"
#include "policy_rv.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>

#include <fcntl.h>

using std::string;


namespace l
{
  static
  void
  utimens_loop_core(const string   &basepath_,
                    const char     *fusepath_,
                    const timespec  ts_[2],
                    PolicyRV       *prv_)
  {
    string fullpath;

    fullpath = fs::path::make(basepath_,fusepath_);

    errno = 0;
    fs::lutimens(fullpath,ts_);

    prv_->insert(errno,basepath_);
  }

  static
  void
  utimens_loop(const std::vector<Branch*> &branches_,
               const char                 *fusepath_,
               const timespec              ts_[2],
               PolicyRV                   *prv_)
  {
    for(auto &branch : branches_)
      {
        l::utimens_loop_core(branch->path,fusepath_,ts_,prv_);
      }
  }

  static
  int
  utimens(const Policy::Action &utimensPolicy_,
          const Policy::Search &getattrPolicy_,
          const Branches       &branches_,
          const char           *fusepath_,
          const timespec        ts_[2])
  {
    int rv;
    PolicyRV prv;
    std::vector<Branch*> branches;

    rv = utimensPolicy_(branches_,fusepath_,branches);
    if(rv == -1)
      return -errno;

    l::utimens_loop(branches,fusepath_,ts_,&prv);
    if(prv.errors.empty())
      return 0;
    if(prv.successes.empty())
      return prv.errors[0].rv;

    branches.clear();
    rv = getattrPolicy_(branches_,fusepath_,branches);
    if(rv == -1)
      return -errno;

    return prv.get_error(branches[0]->path);
  }
}

namespace FUSE
{
  int
  utimens(const char     *fusepath_,
          const timespec  ts_[2])
  {
    Config::Read cfg;
    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::utimens(cfg->func.utimens.policy,
                      cfg->func.getattr.policy,
                      cfg->branches,
                      fusepath_,
                      ts_);
  }
}
