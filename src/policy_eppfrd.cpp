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

#include "policy_eppfrd.hpp"

#include "errno.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "policy.hpp"
#include "policy_eppfrd.hpp"
#include "policy_error.hpp"
#include "rnd.hpp"
#include "rwlock.hpp"
#include "strvec.hpp"

#include <string>
#include <vector>

struct BranchInfo
{
  u64     spaceavail;
  Branch *branch;
};

typedef std::vector<BranchInfo> BranchInfoVec;

static
int
_get_branchinfo_create(const Branches::Ptr &branches_,
                       const fs::path      &fusepath_,
                       BranchInfoVec       *branchinfo_,
                       uint64_t            *sum_)
{
  int rv;
  int error;
  fs::info_t info;

  *sum_ = 0;
  error = ENOENT;
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

      *sum_ += info.spaceavail;

      branchinfo_->push_back({info.spaceavail,&branch});
    }

  return -error;
}

static
int
_get_branchinfo_action(const Branches::Ptr &branches_,
                       const fs::path      &fusepath_,
                       BranchInfoVec       *branchinfo_,
                       uint64_t            *sum_)
{
  int rv;
  int error;
  fs::info_t info;

  *sum_ = 0;
  error = ENOENT;
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

      *sum_ += info.spaceavail;

      branchinfo_->push_back({info.spaceavail,&branch});
    }

  return -error;
}

static
int
_get_branchinfo_search(const Branches::Ptr &branches_,
                       const fs::path      &fusepath_,
                       BranchInfoVec       *branchinfo_,
                       uint64_t            *sum_)
{
  int rv;
  uint64_t spaceavail;

  *sum_ = 0;
  for(auto &branch : *branches_)
    {
      if(!fs::exists(branch.path,fusepath_))
        continue;
      rv = fs::statvfs_cache_spaceavail(branch.path,&spaceavail);
      if(rv < 0)
        continue;

      *sum_ += spaceavail;

      branchinfo_->push_back({spaceavail,&branch});
    }

  return -ENOENT;
}

static
Branch*
_get_branch(const BranchInfoVec &branchinfo_,
            const uint64_t       sum_)
{
  uint64_t idx;
  uint64_t threshold;

  if(sum_ == 0)
    return NULL;

  idx = 0;
  threshold = RND::rand64(sum_);
  for(size_t i = 0; i < branchinfo_.size(); i++)
    {
      idx += branchinfo_[i].spaceavail;

      if(idx < threshold)
        continue;

      return branchinfo_[i].branch;
    }

  return NULL;
}

static
int
_create(const Branches::Ptr  &branches_,
        const fs::path       &fusepath_,
        std::vector<Branch*> &paths_)
{
  int err;
  uint64_t sum;
  Branch *branch;
  BranchInfoVec branchinfo;

  err    = ::_get_branchinfo_create(branches_,fusepath_,&branchinfo,&sum);
  branch = ::_get_branch(branchinfo,sum);
  if(!branch)
    return err;

  paths_.emplace_back(branch);

  return 0;
}

static
int
_action(const Branches::Ptr  &branches_,
        const fs::path       &fusepath_,
        std::vector<Branch*> &paths_)
{
  int err;
  uint64_t sum;
  Branch *branch;
  BranchInfoVec branchinfo;

  err    = ::_get_branchinfo_action(branches_,fusepath_,&branchinfo,&sum);
  branch = ::_get_branch(branchinfo,sum);
  if(!branch)
    return err;

  paths_.emplace_back(branch);

  return 0;
}

static
int
_search(const Branches::Ptr  &branches_,
        const fs::path       &fusepath_,
        std::vector<Branch*> &paths_)
{
  int err;
  uint64_t sum;
  Branch *branch;
  BranchInfoVec branchinfo;

  err    = ::_get_branchinfo_search(branches_,fusepath_,&branchinfo,&sum);
  branch = ::_get_branch(branchinfo,sum);
  if(!branch)
    return err;

  paths_.emplace_back(branch);

  return 0;
}

int
Policy::EPPFRD::Action::operator()(const Branches::Ptr  &branches_,
                                   const fs::path       &fusepath_,
                                   std::vector<Branch*> &paths_) const
{
  return ::_action(branches_,fusepath_,paths_);
}

int
Policy::EPPFRD::Create::operator()(const Branches::Ptr  &branches_,
                                   const fs::path       &fusepath_,
                                   std::vector<Branch*> &paths_) const
{
  return ::_create(branches_,fusepath_,paths_);
}

int
Policy::EPPFRD::Search::operator()(const Branches::Ptr  &branches_,
                                   const fs::path       &fusepath_,
                                   std::vector<Branch*> &paths_) const
{
  return ::_search(branches_,fusepath_,paths_);
}
