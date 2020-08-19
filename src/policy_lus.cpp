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
#include "policy.hpp"
#include "policy_error.hpp"
#include "rwlock.hpp"

#include <limits>
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace lus
{
  static
  int
  create(const Branches &branches_,
         const uint64_t  minfreespace_,
         vector<string> *paths_)
  {
    rwlock::ReadGuard guard(&branches_.lock);

    int rv;
    int error;
    uint64_t lus;
    fs::info_t info;
    const Branch *branch;
    const string *lusbasepath;

    error = ENOENT;
    lus = std::numeric_limits<uint64_t>::max();
    lusbasepath = NULL;
    for(size_t i = 0, ei = branches_.size(); i != ei; i++)
      {
        branch = &branches_[i];

        if(branch->ro_or_nc())
          error_and_continue(error,EROFS);
        rv = fs::info(branch->path,&info);
        if(rv == -1)
          error_and_continue(error,ENOENT);
        if(info.readonly)
          error_and_continue(error,EROFS);
        if(info.spaceavail < minfreespace_)
          error_and_continue(error,ENOSPC);
        if(info.spaceused >= lus)
          continue;

        lus = info.spaceused;
        lusbasepath = &branch->path;
      }

    if(lusbasepath == NULL)
      return (errno=error,-1);

    paths_->push_back(*lusbasepath);

    return 0;
  }
}

int
Policy::Func::lus(const Category  type_,
                  const Branches &branches_,
                  const char     *fusepath_,
                  const uint64_t  minfreespace_,
                  vector<string> *paths_)
{
  if(type_ == Category::CREATE)
    return lus::create(branches_,minfreespace_,paths_);

  return Policy::Func::eplus(type_,branches_,fusepath_,minfreespace_,paths_);
}
