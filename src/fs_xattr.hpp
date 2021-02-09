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
#include <map>


namespace fs
{
  namespace xattr
  {
    using std::string;
    using std::vector;
    using std::map;


    int list(const string   &path,
             vector<char>   *attrs);
    int list(const string   &path,
             string         *attrs);
    int list(const string   &path,
             vector<string> *attrs);

    int get(const string &path,
            const string &attr,
            vector<char> *value);
    int get(const string &path,
            const string &attr,
            string       *value);

    int get(const string       &path,
            map<string,string> *attrs);

    int set(const string &path,
            const string &key,
            const string &value,
            const int     flags);
    int set(const int     fd,
            const string &key,
            const string &value,
            const int     flags);

    int set(const string             &path,
            const map<string,string> &attrs);

    int copy(const int fdin,
             const int fdout);
    int copy(const string &from,
             const string &to);
  }
}
