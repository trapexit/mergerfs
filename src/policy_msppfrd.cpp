/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "policy_msppfrd.hpp"

#include "errno.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "policy.hpp"
#include "policy_msppfrd.hpp"
#include "policies.hpp"
#include "policy_error.hpp"
#include "rnd.hpp"

#include <vector>


struct BranchInfo
{
  u64     spaceavail;
  Branch *branch;
};

typedef std::vector<BranchInfo> BranchInfoVec;

static
int
_create_1(const Branches::Ptr &branches_,
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
_get_branchinfo(const Branches::Ptr &branches_,
                const fs::path      &fusepath_,
                BranchInfoVec       *branchinfo_,
                uint64_t            *sum_)
{
  int rv;
  fs::path fusepath;

  fusepath = fusepath_;
  for(;;)
    {
      rv = ::_create_1(branches_,fusepath,branchinfo_,sum_);
      if(branchinfo_->size())
        break;
      if(fusepath == "/")
        break;
      fusepath = fusepath.parent_path();
    }

  return rv;
}

static
Branch*
_get_branch(const BranchInfoVec &branchinfo_,
            const uint64_t       sum_)
{
  uint64_t idx;
  uint64_t threshold;

  if(sum_ == 0)
    return nullptr;

  idx = 0;
  threshold = RND::rand64(sum_);
  for(const auto &branchinfo : branchinfo_)
    {
      idx += branchinfo.spaceavail;

      if(idx < threshold)
        continue;

      return branchinfo.branch;
    }

  return nullptr;
}

static
int
_create(const Branches::Ptr  &branches_,
        const fs::path       &fusepath_,
        std::vector<Branch*> &paths_)
{
  int rv;
  uint64_t sum;
  Branch *branch;
  BranchInfoVec branchinfo;

  rv     = ::_get_branchinfo(branches_,fusepath_,&branchinfo,&sum);
  branch = ::_get_branch(branchinfo,sum);
  if(!branch)
    return rv;

  paths_.emplace_back(branch);

  return 0;
}

int
Policy::MSPPFRD::Action::operator()(const Branches::Ptr  &branches_,
                                    const fs::path       &fusepath_,
                                    std::vector<Branch*> &paths_) const
{
  return Policies::Action::eppfrd(branches_,fusepath_,paths_);
}

int
Policy::MSPPFRD::Create::operator()(const Branches::Ptr  &branches_,
                                    const fs::path       &fusepath_,
                                    std::vector<Branch*> &paths_) const
{
  return ::_create(branches_,fusepath_,paths_);
}

int
Policy::MSPPFRD::Search::operator()(const Branches::Ptr  &branches_,
                                    const fs::path       &fusepath_,
                                    std::vector<Branch*> &paths_) const
{
  return Policies::Search::eppfrd(branches_,fusepath_,paths_);
}
