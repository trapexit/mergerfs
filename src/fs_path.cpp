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

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "fs_path.hpp"

using std::string;

namespace fs
{
  namespace path
  {
    string
    dirname(const char *path_)
    {
      string path(path_);

      return fs::path::dirname(path);
    }

    string
    dirname(const string &path_)
    {
      string rv;
      string::reverse_iterator i;
      string::reverse_iterator ei;

      rv = path_;

      i  = rv.rbegin();
      ei = rv.rend();
      while(*i == '/' && i != ei)
        i++;

      while(*i != '/' && i != ei)
        i++;

      while(*i == '/' && i != ei)
        i++;

      rv.erase(i.base(),rv.end());

      return rv;
    }

    string
    basename(const string &path)
    {
      return path.substr(path.find_last_of('/')+1);
    }
  }
}
