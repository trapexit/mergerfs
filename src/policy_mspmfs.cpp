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
#include "fs.hpp"
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
  create_1(const Branches &branches_,
           const string   &fusepath_,
           const uint64_t  minfreespace_,
           int            *err_)
  {
    int rv;
    uint64_t mfs;
    string fusepath;
    fs::info_t info;
    const Branch *branch;
    const string *basepath;

    basepath = NULL;
    mfs = std::numeric_limits<uint64_t>::min();
    for(size_t i = 0, ei = branches_.size(); i != ei; i++)
      {
        branch = &branches_[i];

        if(!fs::exists(branch->path,fusepath))
          error_and_continue(*err_,ENOENT);
        if(branch->ro_or_nc())
          error_and_continue(*err_,EROFS);
        rv = fs::info(&branch->path,&info);
        if(rv == -1)
          error_and_continue(*err_,ENOENT);
        if(info.readonly)
          error_and_continue(*err_,EROFS);
        if(info.spaceavail < minfreespace_)
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
  create(const Branches &branches_,
         const char     *fusepath_,
         const uint64_t  minfreespace_,
         vector<string> *paths_)
  {
    int error;
    string fusepath;
    const string *basepath;
    rwlock::ReadGuard guard(&branches_.lock);

    error = ENOENT;
    fusepath = fusepath_;
    do
      {
        basepath = create_1(branches_,fusepath,minfreespace_,&error);
        if(basepath)
          break;

        fusepath = fs::path::dirname(fusepath);
      }
    while(!fusepath.empty());

    if(basepath == NULL)
      return (errno=error,-1);

    paths_->push_back(*basepath);

    return 0;
  }
}

int
Policy::Func::mspmfs(const Category::Enum::Type  type_,
                     const Branches             &branches_,
                     const char                 *fusepath_,
                     const uint64_t              minfreespace_,
                     vector<string>             *paths_)
{
  if(type_ == Category::Enum::create)
    return mspmfs::create(branches_,fusepath_,minfreespace_,paths_);

  return Policy::Func::epmfs(type_,branches_,fusepath_,minfreespace_,paths_);
}
