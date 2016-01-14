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

#ifndef __FS_PATH_HPP__
#define __FS_PATH_HPP__

#include <string>
#include <vector>

namespace fs
{
  namespace path
  {
    using std::string;
    using std::vector;

    string dirname(const string &path);
    string basename(const string &path);

    bool is_empty(const string &path);

    bool exists(vector<string>::const_iterator  begin,
                vector<string>::const_iterator  end,
                const string                   &fusepath);
    bool exists(const vector<string>           &srcmounts,
                const string                   &fusepath);

    inline
    string
    make(const string &base,
         const string &suffix)
    {
      return base + suffix;
    }

    inline
    void
    make(const string &base,
         const string &suffix,
         string       &output)
    {
      output = base + suffix;
    }

    inline
    void
    append(string       &base,
           const string &suffix)
    {
      base += suffix;
    }
  }
};

#endif // __FS_PATH_HPP__
