/*
  Copyright (c) 2019, Antonio SJ Musumeci <trapexit@spawn.link>

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
#include "fs_path.hpp"
#include "fs_unlink.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>
#include <vector>

#include <unistd.h>


namespace error
{
  static
  inline
  int
  calc(const int rv_,
       const int prev_,
       const int cur_)
  {
    if(prev_ != 0)
      return prev_;
    if(rv_ == -1)
      return cur_;
    return 0;
  }
}

namespace l
{
  static
  int
  unlink_loop_core(const std::string &branch_,
                   const char        *fusepath_,
                   const int          error_)
  {
    int rv;
    std::string fullpath;

    fullpath = fs::path::make(branch_,fusepath_);

    rv = fs::unlink(fullpath);

    return error::calc(rv,error_,errno);
  }

  static
  int
  unlink_loop(const std::vector<Branch*> &branches_,
              const char                 *fusepath_)
  {
    int error;

    error = 0;
    for(auto &branch : branches_)
      {
        error = l::unlink_loop_core(branch->path,fusepath_,error);
      }

    return -error;
  }

  static
  int
  unlink(const Policy::Action &unlinkPolicy_,
         const Branches       &branches_,
         const char           *fusepath_)
  {
    int rv;
    std::vector<Branch*> branches;

    rv = unlinkPolicy_(branches_,fusepath_,branches);
    if(rv == -1)
      return -errno;

    return l::unlink_loop(branches,fusepath_);
  }
}

namespace FUSE
{
  int
  unlink(const char *fusepath_)
  {
    Config::Read cfg;
    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::unlink(cfg->func.unlink.policy,
                     cfg->branches,
                     fusepath_);
  }
}
