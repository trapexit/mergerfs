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

#include "errno.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "policy.hpp"
#include "policy_error.hpp"
#include "rnd.hpp"
#include "rwlock.hpp"

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

namespace msppfrd
{
  static
  int
  create_1(const BranchVec &branches_,
           const string    &fusepath_,
           BranchInfoVec   *branchinfo_,
           uint64_t        *sum_)
  {
    int rv;
    int error;
    BranchInfo bi;
    fs::info_t info;
    const Branch *branch;

    *sum_ = 0;
    error = ENOENT;
    for(size_t i = 0, ei = branches_.size(); i < ei; i++)
      {
        branch = &branches_[i];

        if(branch->ro_or_nc())
          error_and_continue(error,EROFS);
        if(!fs::exists(branch->path,fusepath_))
          error_and_continue(error,ENOENT);
        rv = fs::info(branch->path,&info);
        if(rv == -1)
          error_and_continue(error,ENOENT);
        if(info.readonly)
          error_and_continue(error,EROFS);
        if(info.spaceavail < branch->minfreespace())
          error_and_continue(error,ENOSPC);

        *sum_ += info.spaceavail;

        bi.spaceavail = info.spaceavail;
        bi.basepath   = &branch->path;
        branchinfo_->push_back(bi);
      }

    return error;
  }

  static
  int
  create_1(const Branches &branches_,
           const string   &fusepath_,
           BranchInfoVec  *branchinfo_,
           uint64_t       *sum_)
  {
    rwlock::ReadGuard guard(branches_.lock);

    branchinfo_->reserve(branches_.vec.size());

    return msppfrd::create_1(branches_.vec,fusepath_,branchinfo_,sum_);
  }

  static
  int
  get_branchinfo(const Branches &branches_,
                 const char     *fusepath_,
                 BranchInfoVec  *branchinfo_,
                 uint64_t       *sum_)
  {
    int error;
    string fusepath;

    fusepath = fusepath_;
    for(;;)
      {
        error = msppfrd::create_1(branches_,fusepath,branchinfo_,sum_);
        if(branchinfo_->size())
          break;
        if(fusepath == "/")
          break;
        fusepath = fs::path::dirname(fusepath);
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
  create(const Branches &branches_,
         const char     *fusepath_,
         vector<string> *paths_)
  {
    int error;
    uint64_t sum;
    const string *basepath;
    BranchInfoVec branchinfo;

    error    = msppfrd::get_branchinfo(branches_,fusepath_,&branchinfo,&sum);
    basepath = msppfrd::get_branch(branchinfo,sum);
    if(basepath == NULL)
      return (errno=error,-1);

    paths_->push_back(*basepath);

    return 0;
  }
}

int
Policy::Func::msppfrd(const Category  type_,
                      const Branches &branches_,
                      const char     *fusepath_,
                      vector<string> *paths_)
{
  if(type_ == Category::CREATE)
    return msppfrd::create(branches_,fusepath_,paths_);

  return Policy::Func::eppfrd(type_,branches_,fusepath_,paths_);
}
