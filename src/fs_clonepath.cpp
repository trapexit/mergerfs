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
