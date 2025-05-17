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
#include "fs_info.hpp"
#include "fs_path.hpp"
#include "policies.hpp"
#include "policy.hpp"
#include "policy_error.hpp"
#include "policy_pfrd.hpp"
#include "rnd.hpp"
#include "strvec.hpp"

#include <string>
#include <vector>


struct BranchInfo
{
  uint64_t  spaceavail;
  Branch   *branch;
};

typedef std::vector<BranchInfo> BranchInfoVec;

namespace pfrd
{
  static
  int
  get_branchinfo(const Branches::Ptr &branches_,
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
        rv = fs::info(branch.path,&info);
        if(rv == -1)
          error_and_continue(error,ENOENT);
        if(info.readonly)
          error_and_continue(error,EROFS);
        if(info.spaceavail < branch.minfreespace())
          error_and_continue(error,ENOSPC);

        *sum_ += info.spaceavail;

        branchinfo_->push_back({info.spaceavail,&branch});
      }

    return error;
  }

  static
  Branch*
  get_branch(const BranchInfoVec &branchinfo_,
             const uint64_t       sum_)
  {
    uint64_t idx;
    uint64_t threshold;

    if(sum_ == 0)
      return nullptr;

    idx = 0;
    threshold = RND::rand64(sum_);
    for(auto &bi : branchinfo_)
      {
        idx += bi.spaceavail;

        if(idx < threshold)
          continue;

        return bi.branch;
      }

    return nullptr;
  }

  static
  int
  create(const Branches::Ptr  &branches_,
         const char           *fusepath_,
         std::vector<Branch*> &paths_)
  {
    int error;
    uint64_t sum;
    Branch *branch;
    BranchInfoVec branchinfo;

    error  = pfrd::get_branchinfo(branches_,&branchinfo,&sum);
    branch = pfrd::get_branch(branchinfo,sum);
    if(!branch)
      return (errno=error,-1);

    paths_.emplace_back(branch);

    return 0;
  }
}

int
Policy::PFRD::Action::operator()(const Branches::Ptr  &branches_,
                                 const char           *fusepath_,
                                 std::vector<Branch*> &paths_) const
{
  return Policies::Action::eppfrd(branches_,fusepath_,paths_);
}

int
Policy::PFRD::Create::operator()(const Branches::Ptr  &branches_,
                                 const char           *fusepath_,
                                 std::vector<Branch*> &paths_) const
{
  return ::pfrd::create(branches_,fusepath_,paths_);
}

int
Policy::PFRD::Search::operator()(const Branches::Ptr  &branches_,
                                 const char           *fusepath_,
                                 std::vector<Branch*> &paths_) const
{
  return Policies::Search::eppfrd(branches_,fusepath_,paths_);
}
