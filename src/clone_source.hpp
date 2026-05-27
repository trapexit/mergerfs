/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#pragma once

#include "branches.hpp"
#include "fs_exists.hpp"
#include "fs_lstat.hpp"
#include "fs_path.hpp"


namespace CloneSource
{
  // Look up a clone source for path_ across branches_, honoring the
  // configured func.getattr.policy when it has observable side-effects
  // ("newest" picks branch with the newest mtime; "cdco"/"cdfo"/"ff"
  // all degenerate to first-found which matches the cheap path).
  // If policy_name_ is "newest", picks the branch holding the newest
  // copy of path_; otherwise returns the first branch where path_ exists.
  static
  inline
  const Branch*
  find(const Branches::Impl &branches_,
       const fs::path       &path_,
       const std::string    &policy_name_)
  {
    if(policy_name_ == "newest")
      {
        const Branch *best = nullptr;
        struct timespec newest = {0,0};
        struct stat st;
        for(const auto &b : branches_)
          {
            if(!fs::exists(b.path,path_,&st))
              continue;
            if(best
               && ((st.st_mtim.tv_sec  <  newest.tv_sec)
                   || ((st.st_mtim.tv_sec == newest.tv_sec)
                       && (st.st_mtim.tv_nsec < newest.tv_nsec))))
              continue;
            newest = st.st_mtim;
            best = &b;
          }
        return best;
      }

    for(const auto &b : branches_)
      {
        if(fs::exists(b.path,path_))
          return &b;
      }
    return nullptr;
  }
}
