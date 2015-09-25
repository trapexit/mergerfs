/*
  The MIT License (MIT)

  Copyright (c) 2015 Antonio SJ Musumeci <trapexit@spawn.link>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
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
