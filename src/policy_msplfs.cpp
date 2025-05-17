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

#include "errno.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "policies.hpp"
#include "policy.hpp"
#include "policy_error.hpp"
#include "policy_msplfs.hpp"
#include "strvec.hpp"

#include <limits>
#include <string>


namespace msplfs
{
  static
  Branch*
  create_1(const Branches::Ptr &branches_,
           const std::string   &fusepath_,
           int                 *err_)
  {
    int rv;
    uint64_t lfs;
    fs::info_t info;
    Branch *obranch;

    obranch = nullptr;
    lfs = std::numeric_limits<uint64_t>::max();
    for(auto &branch : *branches_)
      {
        if(branch.ro_or_nc())
          error_and_continue(*err_,EROFS);
        if(!fs::exists(branch.path,fusepath_))
          error_and_continue(*err_,ENOENT);
        rv = fs::info(branch.path,&info);
        if(rv == -1)
          error_and_continue(*err_,ENOENT);
        if(info.readonly)
          error_and_continue(*err_,EROFS);
        if(info.spaceavail < branch.minfreespace())
          error_and_continue(*err_,ENOSPC);
        if(info.spaceavail > lfs)
          continue;

        lfs = info.spaceavail;
        obranch = &branch;
      }

    return obranch;
  }

  static
  int
  create(const Branches::Ptr  &branches_,
         const char           *fusepath_,
         std::vector<Branch*> &paths_)
  {
    int error;
    Branch *branch;
    std::string fusepath;

    error = ENOENT;
    fusepath = fusepath_;
    for(;;)
      {
        branch = msplfs::create_1(branches_,fusepath,&error);
        if(branch)
          break;
        if(fusepath == "/")
          break;
        fusepath = fs::path::dirname(fusepath);
      }

    if(!branch)
      return (errno=error,-1);

    paths_.push_back(branch);

    return 0;
  }
}

int
Policy::MSPLFS::Action::operator()(const Branches::Ptr  &branches_,
                                   const char           *fusepath_,
                                   std::vector<Branch*> &paths_) const
{
  return Policies::Action::eplfs(branches_,fusepath_,paths_);
}

int
Policy::MSPLFS::Create::operator()(const Branches::Ptr  &branches_,
                                   const char           *fusepath_,
                                   std::vector<Branch*> &paths_) const
{
  return ::msplfs::create(branches_,fusepath_,paths_);
}

int
Policy::MSPLFS::Search::operator()(const Branches::Ptr  &branches_,
                                   const char           *fusepath_,
                                   std::vector<Branch*> &paths_) const
{
  return Policies::Search::eplfs(branches_,fusepath_,paths_);
}
