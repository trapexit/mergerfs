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

using std::string;
using std::vector;

struct BranchInfo
{
  uint64_t      spaceavail;
  const string *basepath;
};

typedef vector<BranchInfo> BranchInfoVec;

namespace pfrd
{
  static
  int
  get_branchinfo(const Branches::CPtr &branches_,
                 BranchInfoVec        *branchinfo_,
                 uint64_t             *sum_)
  {
    int rv;
    int error;
    BranchInfo bi;
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

        bi.spaceavail = info.spaceavail;
        bi.basepath   = &branch.path;
        branchinfo_->push_back(bi);
      }

    return error;
  }

  static
  const
  string*
  get_branch(const BranchInfoVec &branchinfo_,
             const uint64_t       sum_)
  {
    uint64_t idx;
    uint64_t threshold;

    idx = 0;
    threshold = RND::rand64(sum_);
    for(size_t i = 0; i < branchinfo_.size(); i++)
      {
        idx += branchinfo_[i].spaceavail;

        if(idx < threshold)
          continue;

        return branchinfo_[i].basepath;
      }

    return NULL;
  }

  static
  int
  create(const Branches::CPtr &branches_,
         const char           *fusepath_,
         StrVec               *paths_)
  {
    int error;
    uint64_t sum;
    const string *basepath;
    BranchInfoVec branchinfo;

    error    = pfrd::get_branchinfo(branches_,&branchinfo,&sum);
    basepath = pfrd::get_branch(branchinfo,sum);
    if(basepath == NULL)
      return (errno=error,-1);

    paths_->push_back(*basepath);

    return 0;
  }
}

int
Policy::PFRD::Action::operator()(const Branches::CPtr &branches_,
                                 const char           *fusepath_,
                                 StrVec               *paths_) const
{
  return Policies::Action::eppfrd(branches_,fusepath_,paths_);
}

int
Policy::PFRD::Create::operator()(const Branches::CPtr &branches_,
                                 const char           *fusepath_,
                                 StrVec               *paths_) const
{
  return ::pfrd::create(branches_,fusepath_,paths_);
}

int
Policy::PFRD::Search::operator()(const Branches::CPtr &branches_,
                                 const char           *fusepath_,
                                 StrVec               *paths_) const
{
  return Policies::Search::eppfrd(branches_,fusepath_,paths_);
}
