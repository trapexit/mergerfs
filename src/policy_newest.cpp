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

#include "policy_newest.hpp"

#include "errno.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "policy.hpp"
#include "policy_error.hpp"
#include "policy_newest.hpp"
#include "rwlock.hpp"

#include <string>
#include <limits>

#include <sys/stat.h>

using std::string;

static
int
_create(const Branches::Ptr  &branches_,
        const fs::path       &fusepath_,
        std::vector<Branch*> &paths_)
{
  int rv;
  int err;
  time_t newest;
  struct stat st;
  fs::info_t info;
  Branch *obranch;

  obranch = nullptr;
  err = ENOENT;
  newest = std::numeric_limits<time_t>::min();
  for(auto &branch : *branches_)
    {
      if(branch.ro_or_nc())
        error_and_continue(err,EROFS);
      if(!fs::exists(branch.path,fusepath_,&st))
        error_and_continue(err,ENOENT);
      if(st.st_mtime < newest)
        continue;
      rv = fs::info(branch.path,&info);
      if(rv < 0)
        error_and_continue(err,ENOENT);
      if(info.readonly)
        error_and_continue(err,EROFS);
      if(info.spaceavail < branch.minfreespace())
        error_and_continue(err,ENOSPC);

      newest = st.st_mtime;
      obranch = &branch;
    }

  if(!obranch)
    return -err;

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
  int err;
  bool readonly;
  time_t newest;
  struct stat st;
  Branch *obranch;

  obranch = nullptr;
  err = ENOENT;
  newest = std::numeric_limits<time_t>::min();
  for(auto &branch : *branches_)
    {
      if(branch.ro())
        error_and_continue(err,EROFS);
      if(!fs::exists(branch.path,fusepath_,&st))
        error_and_continue(err,ENOENT);
      if(st.st_mtime < newest)
        continue;
      rv = fs::statvfs_cache_readonly(branch.path,&readonly);
      if(rv < 0)
        error_and_continue(err,ENOENT);
      if(readonly)
        error_and_continue(err,EROFS);

      newest = st.st_mtime;
      obranch = &branch;
    }

  if(!obranch)
    return -err;

  paths_.emplace_back(obranch);

  return 0;
}

static
int
_search(const Branches::Ptr  &branches_,
        const fs::path       &fusepath_,
        std::vector<Branch*> &paths_)
{
  time_t newest;
  struct stat st;
  Branch *obranch;

  obranch = nullptr;
  newest = std::numeric_limits<time_t>::min();
  for(auto &branch : *branches_)
    {
      if(!fs::exists(branch.path,fusepath_,&st))
        continue;
      if(st.st_mtime < newest)
        continue;

      newest = st.st_mtime;
      obranch = &branch;
    }

  if(!obranch)
    return -ENOENT;

  paths_.emplace_back(obranch);

  return 0;
}

int
Policy::Newest::Action::operator()(const Branches::Ptr  &branches_,
                                   const fs::path       &fusepath_,
                                   std::vector<Branch*> &paths_) const
{
  return ::_action(branches_,fusepath_,paths_);
}

int
Policy::Newest::Create::operator()(const Branches::Ptr  &branches_,
                                   const fs::path       &fusepath_,
                                   std::vector<Branch*> &paths_) const
{
  return ::_create(branches_,fusepath_,paths_);
}

int
Policy::Newest::Search::operator()(const Branches::Ptr  &branches_,
                                   const fs::path       &fusepath_,
                                   std::vector<Branch*> &paths_) const
{
  return ::_search(branches_,fusepath_,paths_);
}
