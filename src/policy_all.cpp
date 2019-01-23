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
#include "policy.hpp"
#include "policy_error.hpp"

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace all
{
  static
  int
  create(const Branches        &branches_,
         const uint64_t         minfreespace,
         vector<const string*> &paths)
  {
    int rv;
    int error;
    fs::info_t info;
    const Branch *branch;

    error = ENOENT;
    for(size_t i = 0, ei = branches_.size(); i != ei; i++)
      {
        branch = &branches_[i];

        if(branch->ro_or_nc())
          error_and_continue(error,EROFS);
        rv = fs::info(&branch->path,&info);
        if(rv == -1)
          error_and_continue(error,ENOENT);
        if(info.readonly)
          error_and_continue(error,EROFS);
        if(info.spaceavail < minfreespace)
          error_and_continue(error,ENOSPC);

        paths.push_back(&branch->path);
      }

    if(paths.empty())
      return (errno=error,-1);

    return 0;
  }
}

int
Policy::Func::all(const Category::Enum::Type  type,
                  const Branches             &branches_,
                  const char                 *fusepath,
                  const uint64_t              minfreespace,
                  vector<const string*>      &paths)
{
  if(type == Category::Enum::create)
    return all::create(branches_,minfreespace,paths);

  return Policy::Func::epall(type,branches_,fusepath,minfreespace,paths);
}
