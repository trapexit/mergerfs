/*
  ISC License

  Copyright (c) 2022, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "stat_func.hpp"

#include "fs_lstat.hpp"
#include "fs_stat.hpp"

namespace gfs = ghc::filesystem;

namespace l
{
  static
  void
  stat_if_leads_to_dir(const gfs::path &fullpath_,
                       struct stat     *st_)
  {
    int rv;
    struct stat st;

    rv = fs::stat(fullpath_,&st);
    if(rv == -1)
      return;

    if(S_ISDIR(st.st_mode))
      *st_ = st;

    return;
  }

  static
  void
  stat_if_leads_to_reg(const gfs::path &fullpath_,
                       struct stat     *st_)
  {
    int rv;
    struct stat st;

    rv = fs::stat(fullpath_,&st);
    if(rv == -1)
      return;

    if(S_ISREG(st.st_mode))
      *st_ = st;

    return;
  }
}

int
Stat::no_follow(const gfs::path &fullpath_,
                struct stat     *st_)
{
  return fs::lstat(fullpath_,st_);
}

int
Stat::follow_all(const gfs::path &fullpath_,
                 struct stat     *st_)
{
  int rv;

  rv = fs::stat(fullpath_,st_);
  if(rv == -1)
    rv = fs::lstat(fullpath_,st_);

  return rv;
}

int
Stat::follow_dir(const gfs::path &fullpath_,
                 struct stat     *st_)
{
  int rv;

  rv = fs::lstat(fullpath_,st_);
  if(rv == -1)
    return -1;

  if(S_ISLNK(st_->st_mode))
    l::stat_if_leads_to_dir(fullpath_,st_);

  return 0;
}

int
Stat::follow_reg(const gfs::path &fullpath_,
                 struct stat     *st_)
{
  int rv;

  rv = fs::lstat(fullpath_,st_);
  if(rv == -1)
    return -1;

  if(S_ISLNK(st_->st_mode))
    l::stat_if_leads_to_reg(fullpath_,st_);

  return 0;
}

Stat::Func
Stat::factory(const std::string &str_)
{
  if(str_ == "never")
    return Stat::no_follow;
  if(str_ == "all")
    return Stat::follow_all;
  if(str_ == "regular")
    return Stat::follow_reg;
  if(str_ == "directory")
    return Stat::follow_dir;

  return Stat::no_follow;
}
