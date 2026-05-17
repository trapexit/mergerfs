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

#include "fs_lstat.hpp"
#include "fs_path.hpp"


namespace fs
{
  static
  inline
  bool
  exists(const char  *path_,
         struct stat *st_)
  {
    int rv;

    rv = fs::lstat(path_,st_);

    return (rv == 0);
  }

  static
  inline
  bool
  exists(const char *path_)
  {
    struct stat st;

    return fs::exists(path_,&st);
  }

  static
  inline
  bool
  exists(const std::string &path_,
         struct stat       *st_)
  {
    return fs::exists(path_.c_str(),st_);
  }

  static
  inline
  bool
  exists(const std::string &path_)
  {
    return fs::exists(path_.c_str());
  }

  static
  inline
  bool
  exists(const fs::relpath &path_,
         struct stat       *st_)
  {
    return fs::exists(path_.c_str(),st_);
  }

  static
  inline
  bool
  exists(const fs::relpath &path_)
  {
    return fs::exists(path_.c_str());
  }

  // Two-arg helpers for the per-branch loop pattern: build
  // basepath/'/'/relpath into an fs::relpath and stat it.
  static
  inline
  bool
  exists(const std::string &basepath_,
         const fs::relpath &relpath_,
         struct stat       *st_)
  {
    fs::relpath fullpath;

    fullpath.assign_concat(basepath_,relpath_);

    return fs::exists(fullpath,st_);
  }

  static
  inline
  bool
  exists(const std::string &basepath_,
         const fs::relpath &relpath_)
  {
    struct stat st;

    return fs::exists(basepath_,relpath_,&st);
  }

  // Hot-path variant for per-branch fan-out loops. The caller builds
  // the fullpath once (with the rel portion set) outside the loop, and
  // each iteration only rewrites the prefix area via set_prefix --
  // skipping the per-iteration rel memmove that the (basepath,
  // relpath) overload incurs.
  static
  inline
  bool
  exists(fs::relpath       &fullpath_,
         const std::string &basepath_,
         struct stat       *st_)
  {
    fullpath_.set_prefix(basepath_);
    return fs::exists(fullpath_.c_str(),st_);
  }

  static
  inline
  bool
  exists(fs::relpath       &fullpath_,
         const std::string &basepath_)
  {
    fullpath_.set_prefix(basepath_);
    return fs::exists(fullpath_.c_str());
  }
}
