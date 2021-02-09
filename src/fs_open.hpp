/*
  ISC License

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

#pragma once

#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>


namespace fs
{
  static
  inline
  int
  open(const char *path_,
       const int   flags_)
  {
    return ::open(path_,flags_);
  }

  static
  inline
  int
  open(const char   *path_,
       const int     flags_,
       const mode_t  mode_)
  {
    return ::open(path_,flags_,mode_);
  }

  static
  inline
  int
  open(const std::string &path_,
       const int          flags_)
  {
    return fs::open(path_.c_str(),flags_);
  }

  static
  inline
  int
  open(const std::string &path_,
       const int          flags_,
       const mode_t       mode_)
  {
    return fs::open(path_.c_str(),flags_,mode_);
  }

  static
  inline
  int
  open_dir_ro(const std::string &path_)
  {
    return fs::open(path_,O_RDONLY|O_DIRECTORY);
  }
}
