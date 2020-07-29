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

#include "branch.hpp"
#include "errno.hpp"
#include "fs_base_stat.hpp"
#include "fs_path.hpp"

#include <string>

namespace fs
{
  int
  findonfs(const Branches    &branches_,
           const std::string &fusepath_,
           const int          fd_,
           std::string       *basepath_)
  {
    int rv;
    dev_t dev;
    struct stat st;
    std::string fullpath;
    const Branch *branch;
    const rwlock::ReadGuard guard(&branches_.lock);

    rv = fs::fstat(fd_,&st);
    if(rv == -1)
      return -1;

    dev = st.st_dev;
    for(size_t i = 0, ei = branches_.size(); i != ei; i++)
      {
        branch = &branches_[i];

        fullpath = fs::path::make(branch->path,fusepath_);

        rv = fs::lstat(fullpath,&st);
        if(rv == -1)
          continue;

        if(st.st_dev != dev)
          continue;

        *basepath_ = branch->path;

        return 0;
      }

    return (errno=ENOENT,-1);
  }
}
