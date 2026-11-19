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

#include "fs_findonfs.hpp"

#include "branches.hpp"
#include "errno.hpp"
#include "fs_fstat.hpp"
#include "fs_lstat.hpp"
#include "fs_path.hpp"


static
bool
_is_fd_file(const fs::path    &basepath_,
            const fs::path    &fusepath_,
            const struct stat &fdst_)
{
  int rv;
  struct stat st;

  rv = fs::lstat(basepath_ / fusepath_,&st);
  if(rv < 0)
    return false;

  return ((st.st_dev == fdst_.st_dev) &&
          (st.st_ino == fdst_.st_ino));
}

static
int
_findonfs(const Branches::Ptr &branches_,
          const fs::path      &fusepath_,
          const int            fd_,
          fs::path            *basepath_)
{
  int rv;
  struct stat st;

  rv = fs::fstat(fd_,&st);
  if(rv < 0)
    return rv;

  if(::_is_fd_file(*basepath_,fusepath_,st))
    return 0;

  for(const auto &branch : *branches_)
    {
      if(not ::_is_fd_file(branch.path,fusepath_,st))
        continue;

      *basepath_ = branch.path;

      return 0;
    }

  return -ENOENT;
}


int
fs::findonfs(const Branches::Ptr &branches_,
             const fs::path      &fusepath_,
             const int            fd_,
             fs::path            *basepath_)
{
  return ::_findonfs(branches_,fusepath_,fd_,basepath_);
}
