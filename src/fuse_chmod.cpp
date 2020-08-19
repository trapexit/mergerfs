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
#include "rv.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>
#include <vector>

#include <string.h>

using std::string;
using std::vector;

namespace l
{
  static
  int
  chmod_loop_core(const string &basepath_,
                  const char   *fusepath_,
                  const mode_t  mode_,
                  const int     error_)
  {
    int rv;
    string fullpath;

    fullpath = fs::path::make(basepath_,fusepath_);

    rv = fs::lchmod(fullpath,mode_);

    return error::calc(rv,error_,errno);
  }

  static
  int
  chmod_loop(const vector<string> &basepaths_,
             const char           *fusepath_,
             const mode_t          mode_)
  {
    int error;

    error = -1;
    for(size_t i = 0, ei = basepaths_.size(); i != ei; i++)
      {
        error = l::chmod_loop_core(basepaths_[i],fusepath_,mode_,error);
      }

    return -error;
  }

  static
  int
  chmod(Policy::Func::Action  actionFunc_,
        const Branches       &branches_,
        const uint64_t        minfreespace_,
        const char           *fusepath_,
        const mode_t          mode_)
  {
    int rv;
    vector<string> basepaths;

    rv = actionFunc_(branches_,fusepath_,minfreespace_,&basepaths);
    if(rv == -1)
      return -errno;

    return l::chmod_loop(basepaths,fusepath_,mode_);
  }
}

namespace FUSE
{
  int
  chmod(const char *fusepath_,
        mode_t      mode_)
  {
    const fuse_context *fc     = fuse_get_context();
    const Config       &config = Config::ro();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::chmod(config.func.chmod.policy,
                    config.branches,
                    config.minfreespace,
                    fusepath_,
                    mode_);
  }
}
