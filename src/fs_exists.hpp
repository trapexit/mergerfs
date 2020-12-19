/*
  ISC License

  Copyright (c) 2018, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include <string>


namespace fs
{
  static
  inline
  bool
  exists(const std::string &path_,
         struct stat       *st_)
  {
    int rv;

    rv = fs::lstat(path_,st_);

    return (rv == 0);
  }

  static
  inline
  bool
  exists(const std::string &path_)
  {
    struct stat st;

    return fs::exists(path_,&st);
  }

  static
  inline
  bool
  exists(const std::string &basepath_,
         const std::string &relpath_)
  {
    std::string fullpath;

    fullpath = fs::path::make(basepath_,relpath_);

    return fs::exists(fullpath);
  }

  static
  inline
  bool
  exists(const std::string &basepath_,
         const char        *relpath_,
         struct stat       *st_)
  {
    std::string fullpath;

    fullpath = fs::path::make(basepath_,relpath_);

    return fs::exists(fullpath,st_);
  }

  static
  inline
  bool
  exists(const std::string &basepath_,
         const char        *relpath_)
  {
    struct stat st;

    return fs::exists(basepath_,relpath_,&st);
  }
}
