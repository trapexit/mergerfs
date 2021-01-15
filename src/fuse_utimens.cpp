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
  int
  get_error(const PolicyRV &prv_,
            const string   &basepath_)
  {
    for(int i = 0, ei = prv_.success.size(); i < ei; i++)
      {
        if(prv_.success[i].basepath == basepath_)
          return prv_.success[i].rv;
      }

    for(int i = 0, ei = prv_.error.size(); i < ei; i++)
      {
        if(prv_.error[i].basepath == basepath_)
          return prv_.error[i].rv;
      }

    return 0;
  }

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
  utimens_loop(const StrVec   &basepaths_,
               const char     *fusepath_,
               const timespec  ts_[2],
               PolicyRV       *prv_)
  {
    for(size_t i = 0, ei = basepaths_.size(); i != ei; i++)
      {
        l::utimens_loop_core(basepaths_[i],fusepath_,ts_,prv_);
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
    StrVec basepaths;

    rv = utimensPolicy_(branches_,fusepath_,&basepaths);
    if(rv == -1)
      return -errno;

    l::utimens_loop(basepaths,fusepath_,ts_,&prv);
    if(prv.error.empty())
      return 0;
    if(prv.success.empty())
      return prv.error[0].rv;

    basepaths.clear();
    rv = getattrPolicy_(branches_,fusepath_,&basepaths);
    if(rv == -1)
      return -errno;

    return l::get_error(prv,basepaths[0]);
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
