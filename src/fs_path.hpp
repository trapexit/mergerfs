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

#pragma once

#include <string>
#include <vector>

namespace fs
{
  namespace path
  {
    using std::string;

    string dirname(const char *path_);
    string dirname(const string *path_);

    string basename(const string &path);

    static
    inline
    void
    append(string     &base,
           const char *suffix)
    {
      base += suffix;
    }

    static
    inline
    void
    append(string       &base,
           const string &suffix)
    {
      base += suffix;
    }

    static
    inline
    string
    make(const string &base_,
         const char   *suffix_)
    {
      return (base_ + suffix_);
    }

    static
    inline
    string
    make(const string *base_,
         const char   *suffix_)
    {
      return (*base_ + suffix_);
    }

    static
    inline
    string
    make(const string *base_,
         const string *suffix_)
    {
      return (*base_ + *suffix_);
    }

    static
    inline
    string
    make(const string &base_,
         const string &suffix_)
    {
      return (base_ + suffix_);
    }
  }
};
