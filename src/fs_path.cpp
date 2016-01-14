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
    dirname(const string &path)
    {
      string parent = path;
      string::reverse_iterator i;
      string::reverse_iterator bi;

      bi = parent.rend();
      i  = parent.rbegin();
      while(*i == '/' && i != bi)
        i++;

      while(*i != '/' && i != bi)
        i++;

      while(*i == '/' && i != bi)
        i++;

      parent.erase(i.base(),parent.end());

      return parent;
    }

    string
    basename(const string &path)
    {
      return path.substr(path.find_last_of('/')+1);
    }

    bool
    is_empty(const string &path)
    {
      DIR           *dir;
      struct dirent *de;

      dir = ::opendir(path.c_str());
      if(!dir)
        return false;

      while((de = ::readdir(dir)))
        {
          const char *d_name = de->d_name;

          if(d_name[0] == '.'     &&
             ((d_name[1] == '\0') ||
              (d_name[1] == '.' && d_name[2] == '\0')))
            continue;

          ::closedir(dir);

          return false;
        }

      ::closedir(dir);

      return true;
    }

    bool
    exists(const vector<string> &paths,
           const string         &fusepath)
    {
      for(size_t i = 0, ei = paths.size(); i != ei; i++)
        {
          int rv;
          string path;
          struct stat st;

          fs::path::make(paths[i],fusepath,path);

          rv  = ::lstat(path.c_str(),&st);
          if(rv == 0)
            return true;
        }

      return false;
    }
  }
}
