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
#include "policy_newest.hpp"
#include "rwlock.hpp"

#include <string>
#include <limits>

#include <sys/stat.h>

using std::string;

namespace newest
{
  static
  int
  create(const Branches::CPtr &branches_,
         const char           *fusepath_,
         StrVec               *paths_)
  {
    int rv;
    int error;
    time_t newest;
    struct stat st;
    fs::info_t info;
    const string *basepath;

    error = ENOENT;
    newest = std::numeric_limits<time_t>::min();
    basepath = NULL;
    for(auto &branch : *branches_)
      {
        if(branch.ro_or_nc())
          error_and_continue(error,EROFS);
        if(!fs::exists(branch.path,fusepath_,&st))
          error_and_continue(error,ENOENT);
        if(st.st_mtime < newest)
          continue;
        rv = fs::info(branch.path,&info);
        if(rv == -1)
          error_and_continue(error,ENOENT);
        if(info.readonly)
          error_and_continue(error,EROFS);
        if(info.spaceavail < branch.minfreespace())
          error_and_continue(error,ENOSPC);

        newest = st.st_mtime;
        basepath = &branch.path;
      }

    if(basepath == NULL)
      return (errno=error,-1);

    paths_->push_back(*basepath);

    return 0;
  }

  static
  int
  action(const Branches::CPtr &branches_,
         const char           *fusepath_,
         StrVec               *paths_)
  {
    int rv;
    int error;
    bool readonly;
    time_t newest;
    struct stat st;
    const string *basepath;

    error = ENOENT;
    newest = std::numeric_limits<time_t>::min();
    basepath = NULL;
    for(auto &branch : *branches_)
      {
        if(branch.ro())
          error_and_continue(error,EROFS);
        if(!fs::exists(branch.path,fusepath_,&st))
          error_and_continue(error,ENOENT);
        if(st.st_mtime < newest)
          continue;
        rv = fs::statvfs_cache_readonly(branch.path,&readonly);
        if(rv == -1)
          error_and_continue(error,ENOENT);
        if(readonly)
          error_and_continue(error,EROFS);

        newest = st.st_mtime;
        basepath = &branch.path;
      }

    if(basepath == NULL)
      return (errno=error,-1);

    paths_->push_back(*basepath);

    return 0;
  }

  static
  int
  search(const Branches::CPtr &branches_,
         const char           *fusepath_,
         StrVec               *paths_)
  {
    time_t newest;
    struct stat st;
    const string *basepath;

    newest = std::numeric_limits<time_t>::min();
    basepath = NULL;
    for(auto &branch : *branches_)
      {
        if(!fs::exists(branch.path,fusepath_,&st))
          continue;
        if(st.st_mtime < newest)
          continue;

        newest = st.st_mtime;
        basepath = &branch.path;
      }

    if(basepath == NULL)
      return (errno=ENOENT,-1);

    paths_->push_back(*basepath);

    return 0;
  }
}

int
Policy::Newest::Action::operator()(const Branches::CPtr &branches_,
                                   const char           *fusepath_,
                                   StrVec               *paths_) const
{
  return ::newest::action(branches_,fusepath_,paths_);
}

int
Policy::Newest::Create::operator()(const Branches::CPtr &branches_,
                                   const char           *fusepath_,
                                   StrVec               *paths_) const
{
  return ::newest::create(branches_,fusepath_,paths_);
}

int
Policy::Newest::Search::operator()(const Branches::CPtr &branches_,
                                   const char           *fusepath_,
                                   StrVec               *paths_) const
{
  return ::newest::search(branches_,fusepath_,paths_);
}
