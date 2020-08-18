/*
  Copyright (c) 2020, LiveIntent ApS, Andreas E. Dalsgaard <andreas.dalsgaard@gmail.com>

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
#include "fs.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_path.hpp"
#include "policy.hpp"
#include "policy_error.hpp"
#include "rwlock.hpp"

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace pfrd
{
  uint64_t get_int64_rand(uint64_t const& min, uint64_t const& max)
  {
    return (((uint64_t)(unsigned int)rand() << 32) + (uint64_t)(unsigned int)rand()) % (max - min) + min;
  }

  static
  int
  create(const Branches &branches_,
         const uint64_t  minfreespace_,
         vector<string> *paths_)
  {
    rwlock::ReadGuard guard(&branches_.lock);

    int rv;
    int error;
    uint64_t sum, index;
    uint64_t byte_index;
    fs::info_t info;
    const Branch *branch;
    const string *pfrdbasepath;
    uint64_t cache_spaceavail_branches[256];
    bool useable_branches[256];
    const string *cache_basepath_branches[256];
    size_t branch_count = branches_.size();

    error = ENOENT;
    pfrdbasepath = NULL;
    sum = 0, index = 0;
    for(size_t i = 0; i != branch_count; i++)
      {
        branch = &branches_[i];

        if(branch->ro_or_nc())
          error_and_continue(error,EROFS);
        rv = fs::info(&branch->path,&info);
        if(rv == -1)
          error_and_continue(error,ENOENT);
        if(info.readonly)
          error_and_continue(error,EROFS);
        if(info.spaceavail < minfreespace_)
          error_and_continue(error,ENOSPC);

        cache_spaceavail_branches[i] = info.spaceavail;
        useable_branches[i] = true;
        cache_basepath_branches[i] = &branch->path;

        sum += info.spaceavail;
      }

    byte_index = get_int64_rand(0, sum);

    for(size_t i = 0; i != branch_count; i++)
      {
        if (!useable_branches[i])
          continue;

        index += cache_spaceavail_branches[i];

        if(index > byte_index) {
          pfrdbasepath = cache_basepath_branches[i];
          break;
        }
      }

    if(pfrdbasepath == NULL)
      return (errno=error,-1);

    paths_->push_back(*pfrdbasepath);

    return 0;
  }
}

int
Policy::Func::pfrd(const Category::Enum::Type  type,
                  const Branches             &branches_,
                  const char                 *fusepath,
                  const uint64_t              minfreespace,
                  vector<string>             *paths)
{
  if(type == Category::Enum::create)
    return pfrd::create(branches_,minfreespace,paths);

  return Policy::Func::epmfs(type,branches_,fusepath,minfreespace,paths);
}
