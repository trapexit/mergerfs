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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <string>

#include "fs_path.hpp"
#include "fs_attr.hpp"
#include "fs_xattr.hpp"

using std::string;

namespace fs
{
  static
  bool
  ignorable_error(const int err)
  {
    switch(err)
      {
      case ENOTTY:
      case ENOTSUP:
#if ENOTSUP != EOPNOTSUPP
      case EOPNOTSUPP:
#endif
        return true;
      }

    return false;
  }

  int
  clonepath(const string &fromsrc,
            const string &tosrc,
            const string &relative)
  {
    int         rv;
    struct stat st;
    string      topath;
    string      frompath;
    string      dirname;

    dirname = fs::path::dirname(relative);
    if(!dirname.empty())
      {
        rv = clonepath(fromsrc,tosrc,dirname);
        if(rv == -1)
          return -1;
      }

    fs::path::make(fromsrc,relative,frompath);
    rv = ::stat(frompath.c_str(),&st);
    if(rv == -1)
      return -1;
    else if(!S_ISDIR(st.st_mode))
      return (errno = ENOTDIR,-1);

    fs::path::make(tosrc,relative,topath);
    rv = ::mkdir(topath.c_str(),st.st_mode);
    if(rv == -1)
      {
        if(errno != EEXIST)
          return -1;

        rv = ::chmod(topath.c_str(),st.st_mode);
        if(rv == -1)
          return -1;
      }

    // It may not support it... it's fine...
    rv = fs::attr::copy(frompath,topath);
    if(rv == -1 && !ignorable_error(errno))
      return -1;

    rv = fs::xattr::copy(frompath,topath);
    if(rv == -1 && !ignorable_error(errno))
      return -1;

    rv = ::chown(topath.c_str(),st.st_uid,st.st_gid);
    if(rv == -1)
      return -1;

    struct timespec times[2];
    times[0] = st.st_atim;
    times[1] = st.st_mtim;
    rv = ::utimensat(-1,topath.c_str(),times,0);
    if(rv == -1)
      return -1;

    return 0;
  }
}
