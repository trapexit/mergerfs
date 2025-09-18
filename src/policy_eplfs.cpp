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

#include "policy_eplfs.hpp"

#include "errno.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "policies.hpp"
#include "policy.hpp"
#include "policy_eplfs.hpp"
#include "policy_error.hpp"

#include <limits>
#include <string>
#include <vector>

using std::string;
using std::vector;

static
int
_create(const Branches::Ptr  &branches_,
        const fs::path       &fusepath_,
        std::vector<Branch*> &paths_)
{
  int rv;
  int error;
  uint64_t eplfs;
  Branch *obranch;
  fs::info_t info;

  obranch = nullptr;
  error = ENOENT;
  eplfs = std::numeric_limits<uint64_t>::max();
  for(auto &branch : *branches_)
    {
      if(branch.ro_or_nc())
        error_and_continue(error,EROFS);
      if(!fs::exists(branch.path,fusepath_))
        error_and_continue(error,ENOENT);
      rv = fs::info(branch.path,&info);
      if(rv < 0)
        error_and_continue(error,ENOENT);
      if(info.readonly)
        error_and_continue(error,EROFS);
      if(info.spaceavail < branch.minfreespace())
        error_and_continue(error,ENOSPC);
      if(info.spaceavail > eplfs)
        continue;

      eplfs = info.spaceavail;
      obranch = &branch;
    }

  if(obranch == nullptr)
    return -error;

  paths_.emplace_back(obranch);

  return 0;
}

static
int
_action(const Branches::Ptr  &branches_,
        const fs::path       &fusepath_,
        std::vector<Branch*> &paths_)
{
  int rv;
  int error;
  uint64_t eplfs;
  Branch *obranch;
  fs::info_t info;

  obranch = nullptr;
  error = ENOENT;
  eplfs = std::numeric_limits<uint64_t>::max();
  for(auto &branch : *branches_)
    {
      if(branch.ro())
        error_and_continue(error,EROFS);
      if(!fs::exists(branch.path,fusepath_))
        error_and_continue(error,ENOENT);
      rv = fs::info(branch.path,&info);
      if(rv < 0)
        error_and_continue(error,ENOENT);
      if(info.readonly)
        error_and_continue(error,EROFS);
      if(info.spaceavail > eplfs)
        continue;

      eplfs = info.spaceavail;
      obranch = &branch;
    }

  if(obranch == nullptr)
    return -error;

  paths_.emplace_back(obranch);

  return 0;
}

static
int
_search(const Branches::Ptr  &branches_,
        const fs::path       &fusepath_,
        std::vector<Branch*> &paths_)
{
  int rv;
  uint64_t eplfs;
  uint64_t spaceavail;
  Branch *obranch;

  obranch = nullptr;
  eplfs = std::numeric_limits<uint64_t>::max();
  for(auto &branch : *branches_)
    {
      if(!fs::exists(branch.path,fusepath_))
        continue;
      rv = fs::statvfs_cache_spaceavail(branch.path,&spaceavail);
      if(rv < 0)
        continue;
      if(spaceavail > eplfs)
        continue;

      eplfs = spaceavail;
      obranch = &branch;
    }

  if(obranch == nullptr)
    return -ENOENT;

  paths_.emplace_back(obranch);

  return 0;
}

int
Policy::EPLFS::Action::operator()(const Branches::Ptr  &branches_,
                                  const fs::path       &fusepath_,
                                  std::vector<Branch*> &paths_) const
{
  return ::_action(branches_,fusepath_,paths_);
}

int
Policy::EPLFS::Create::operator()(const Branches::Ptr &branches_,
                                  const fs::path      &fusepath_,
                                  std::vector<Branch*> &paths_) const
{
  return ::_create(branches_,fusepath_,paths_);
}

int
Policy::EPLFS::Search::operator()(const Branches::Ptr &branches_,
                                  const fs::path      &fusepath_,
                                  std::vector<Branch*> &paths_) const
{
  return ::_search(branches_,fusepath_,paths_);
}
