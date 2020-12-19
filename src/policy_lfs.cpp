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
#include "policies.hpp"
#include "policy.hpp"
#include "policy_error.hpp"
#include "policy_lfs.hpp"
#include "strvec.hpp"

#include <limits>
#include <string>

using std::string;

namespace lfs
{
  static
  int
  create(const Branches::CPtr &branches_,
         StrVec               *paths_)
  {
    int rv;
    int error;
    uint64_t lfs;
    fs::info_t info;
    const Branch *branch;
    const string *basepath;

    error = ENOENT;
    lfs = std::numeric_limits<uint64_t>::max();
    basepath = NULL;
    for(const auto &branch : *branches_)
      {
        if(branch.ro_or_nc())
          error_and_continue(error,EROFS);
        rv = fs::info(branch.path,&info);
        if(rv == -1)
          error_and_continue(error,ENOENT);
        if(info.readonly)
          error_and_continue(error,EROFS);
        if(info.spaceavail < branch.minfreespace())
          error_and_continue(error,ENOSPC);
        if(info.spaceavail > lfs)
          continue;

        lfs = info.spaceavail;
        basepath = &branch.path;
      }

    if(basepath == NULL)
      return (errno=error,-1);

    paths_->push_back(*basepath);

    return 0;
  }
}

int
Policy::LFS::Action::operator()(const Branches::CPtr &branches_,
                                const char           *fusepath_,
                                StrVec               *paths_) const
{
  return Policies::Action::eplfs(branches_,fusepath_,paths_);
}

int
Policy::LFS::Create::operator()(const Branches::CPtr &branches_,
                                const char           *fusepath_,
                                StrVec               *paths_) const
{
  return ::lfs::create(branches_,paths_);
}

int
Policy::LFS::Search::operator()(const Branches::CPtr &branches_,
                                const char           *fusepath_,
                                StrVec               *paths_) const
{
  return Policies::Search::eplfs(branches_,fusepath_,paths_);
}
