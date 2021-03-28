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

#include "errno.hpp"

#include <string>

#include <sys/types.h>
#include <unistd.h>


namespace fs
{
  static
  inline
  int
  truncate(const char  *path_,
           const off_t  length_)
  {
    int rv;

    rv = ::truncate(path_,length_);

    return ((rv == -1) ? -errno : rv);
  }

  static
  inline
  int
  truncate(const std::string &path_,
           const off_t        length_)
  {
    return fs::truncate(path_.c_str(),length_);
  }

  static
  inline
  int
  truncate(const ghc::filesystem::path &path_,
           const off_t                  length_)
  {
    return fs::truncate(path_.c_str(),length_);
  }
}
