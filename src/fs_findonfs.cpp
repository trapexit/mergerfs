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

#include "branches.hpp"
#include "errno.hpp"
#include "fs_fstat.hpp"
#include "fs_lstat.hpp"
#include "fs_path.hpp"

#include <string>


namespace l
{
  static
  int
  findonfs(const Branches::CPtr &branches_,
           const std::string    &fusepath_,
           const int             fd_,
           std::string          *basepath_)
  {
    int rv;
    dev_t dev;
    struct stat st;
    std::string fullpath;

    rv = fs::fstat(fd_,&st);
    if(rv == -1)
      return -1;

    dev = st.st_dev;
    for(const auto &branch : *branches_)
      {
        fullpath = fs::path::make(branch.path,fusepath_);

        rv = fs::lstat(fullpath,&st);
        if(rv == -1)
          continue;

        if(st.st_dev != dev)
          continue;

        *basepath_ = branch.path;

        return 0;
      }

    return (errno=ENOENT,-1);
  }
}

namespace fs
{
  int
  findonfs(const Branches::CPtr &branches_,
           const std::string    &fusepath_,
           const int             fd_,
           std::string          *basepath_)
  {
    return l::findonfs(branches_,fusepath_,fd_,basepath_);
  }
}
