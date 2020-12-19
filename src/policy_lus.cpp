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
#include "policy_lus.hpp"
#include "strvec.hpp"

#include <limits>
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace lus
{
  static
  int
  create(const Branches::CPtr &branches_,
         StrVec               *paths_)
  {
    int rv;
    int error;
    uint64_t lus;
    fs::info_t info;
    const string *basepath;

    error = ENOENT;
    lus = std::numeric_limits<uint64_t>::max();
    basepath = NULL;
    for(auto &branch : *branches_)
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
        if(info.spaceused >= lus)
          continue;

        lus      = info.spaceused;
        basepath = &branch.path;
      }

    if(basepath == NULL)
      return (errno=error,-1);

    paths_->push_back(*basepath);

    return 0;
  }
}

int
Policy::LUS::Action::operator()(const Branches::CPtr &branches_,
                                const char           *fusepath_,
                                StrVec               *paths_) const
{
  return Policies::Action::eplus(branches_,fusepath_,paths_);
}

int
Policy::LUS::Create::operator()(const Branches::CPtr &branches_,
                                const char           *fusepath_,
                                StrVec               *paths_) const
{
  return ::lus::create(branches_,paths_);
}

int
Policy::LUS::Search::operator()(const Branches::CPtr &branches_,
                                const char           *fusepath_,
                                StrVec               *paths_) const
{
  return Policies::Search::eplus(branches_,fusepath_,paths_);
}
