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

#include <string>
#include <vector>
#include <limits>

#include <sys/stat.h>

using std::string;
using std::vector;

namespace newest
{
  static
  int
  create(const BranchVec &branches_,
         const char      *fusepath_,
         vector<string>  *paths_)
  {
    int rv;
    int error;
    time_t newest;
    struct stat st;
    fs::info_t info;
    const Branch *branch;
    const string *basepath;

    error = ENOENT;
    newest = std::numeric_limits<time_t>::min();
    basepath = NULL;
    for(size_t i = 0, ei = branches_.size(); i != ei; i++)
      {
        branch = &branches_[i];

        if(branch->ro_or_nc())
          error_and_continue(error,EROFS);
        if(!fs::exists(branch->path,fusepath_,&st))
          error_and_continue(error,ENOENT);
        if(st.st_mtime < newest)
          continue;
        rv = fs::info(branch->path,&info);
        if(rv == -1)
          error_and_continue(error,ENOENT);
        if(info.readonly)
          error_and_continue(error,EROFS);
        if(info.spaceavail < branch->minfreespace())
          error_and_continue(error,ENOSPC);

        newest = st.st_mtime;
        basepath = &branch->path;
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

    return newest::create(branches_.vec,fusepath_,paths_);
  }

  static
  int
  action(const BranchVec &branches_,
         const char      *fusepath_,
         vector<string>  *paths_)
  {
    int rv;
    int error;
    bool readonly;
    time_t newest;
    struct stat st;
    const Branch *branch;
    const string *basepath;

    error = ENOENT;
    newest = std::numeric_limits<time_t>::min();
    basepath = NULL;
    for(size_t i = 0, ei = branches_.size(); i != ei; i++)
      {
        branch = &branches_[i];

        if(branch->ro())
          error_and_continue(error,EROFS);
        if(!fs::exists(branch->path,fusepath_,&st))
          error_and_continue(error,ENOENT);
        if(st.st_mtime < newest)
          continue;
        rv = fs::statvfs_cache_readonly(branch->path,&readonly);
        if(rv == -1)
          error_and_continue(error,ENOENT);
        if(readonly)
          error_and_continue(error,EROFS);

        newest = st.st_mtime;
        basepath = &branch->path;
      }

    if(basepath == NULL)
      return (errno=error,-1);

    paths_->push_back(*basepath);

    return 0;
  }

  static
  int
  action(const Branches &branches_,
         const char     *fusepath_,
         vector<string> *paths_)
  {
    rwlock::ReadGuard guard(branches_.lock);

    return newest::action(branches_.vec,fusepath_,paths_);
  }

  static
  int
  search(const BranchVec &branches_,
         const char      *fusepath_,
         vector<string>  *paths_)
  {
    time_t newest;
    struct stat st;
    const Branch *branch;
    const string *basepath;

    newest = std::numeric_limits<time_t>::min();
    basepath = NULL;
    for(size_t i = 0, ei = branches_.size(); i != ei; i++)
      {
        branch = &branches_[i];

        if(!fs::exists(branch->path,fusepath_,&st))
          continue;
        if(st.st_mtime < newest)
          continue;

        newest = st.st_mtime;
        basepath = &branch->path;
      }

    if(basepath == NULL)
      return (errno=ENOENT,-1);

    paths_->push_back(*basepath);

    return 0;
  }

  static
  int
  search(const Branches &branches_,
         const char     *fusepath_,
         vector<string> *paths_)
  {
    rwlock::ReadGuard guard(branches_.lock);

    return newest::search(branches_.vec,fusepath_,paths_);
  }
}

int
Policy::Func::newest(const Category  type_,
                     const Branches &branches_,
                     const char     *fusepath_,
                     vector<string> *paths_)
{
  switch(type_)
    {
    case Category::CREATE:
      return newest::create(branches_,fusepath_,paths_);
    case Category::ACTION:
      return newest::action(branches_,fusepath_,paths_);
    case Category::SEARCH:
    default:
      return newest::search(branches_,fusepath_,paths_);
    }
}
