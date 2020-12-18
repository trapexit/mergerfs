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
#include "fs_statvfs_cache.hpp"
#include "policy.hpp"
#include "policy_error.hpp"
#include "rwlock.hpp"

#include <limits>
#include <string>
#include <vector>

using std::string;
using std::vector;


namespace mspmfs
{
  static
  const
  string*
  create_1(const BranchVec &branches_,
           const string    &fusepath_,
           int             *err_)
  {
    int rv;
    uint64_t mfs;
    fs::info_t info;
    const Branch *branch;
    const string *basepath;

    basepath = NULL;
    mfs = std::numeric_limits<uint64_t>::min();
    for(size_t i = 0, ei = branches_.size(); i != ei; i++)
      {
        branch = &branches_[i];

        if(branch->ro_or_nc())
          error_and_continue(*err_,EROFS);
        if(!fs::exists(branch->path,fusepath_))
          error_and_continue(*err_,ENOENT);
        rv = fs::info(branch->path,&info);
        if(rv == -1)
          error_and_continue(*err_,ENOENT);
        if(info.readonly)
          error_and_continue(*err_,EROFS);
        if(info.spaceavail < branch->minfreespace())
          error_and_continue(*err_,ENOSPC);
        if(info.spaceavail < mfs)
          continue;

        mfs = info.spaceavail;
        basepath = &branch->path;
      }

    return basepath;
  }

  static
  int
  create(const BranchVec &branches_,
         const char      *fusepath_,
         vector<string>  *paths_)
  {
    int error;
    string fusepath;
    const string *basepath;

    error = ENOENT;
    fusepath = fusepath_;
    for(;;)
      {
        basepath = mspmfs::create_1(branches_,fusepath,&error);
        if(basepath)
          break;
        if(fusepath == "/")
          break;
        fusepath = fs::path::dirname(fusepath);
      }

    if(basepath == NULL)
      return (errno=error,-1);

    paths_->push_back(*basepath);

    return 0;
  }

  static
  int
  create(const Branches &branches_,
         const char     *fusepath_,
         vector<string> *paths_)
  {
    rwlock::ReadGuard guard(branches_.lock);

    return mspmfs::create(branches_.vec,fusepath_,paths_);
  }
}

int
Policy::Func::mspmfs(const Category  type_,
                     const Branches &branches_,
                     const char     *fusepath_,
                     vector<string> *paths_)
{
  if(type_ == Category::CREATE)
    return mspmfs::create(branches_,fusepath_,paths_);

  return Policy::Func::epmfs(type_,branches_,fusepath_,paths_);
}
