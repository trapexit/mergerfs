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

#include <string>

#include "errno.h"
#include "fs_attr.hpp"
#include "fs_base_chmod.hpp"
#include "fs_base_chown.hpp"
#include "fs_base_mkdir.hpp"
#include "fs_base_stat.hpp"
#include "fs_base_utime.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "fs_xattr.hpp"
#include "ugid.hpp"

using std::string;

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

namespace fs
{
  /*
    Attempts to clone a path.
    The directories which already exist are left alone.
    The new directories have metadata set to match the original if
    possible. Optionally ignore errors on metadata copies.
   */
  int
  clonepath(const string &fromsrc,
            const string &tosrc,
            const char   *relative,
            const bool    return_metadata_errors)
  {
    int         rv;
    struct stat st;
    string      topath;
    string      frompath;
    string      dirname;

    if((relative == NULL) || (relative[0] == '\0'))
      return 0;

    dirname = fs::path::dirname(relative);
    if(!dirname.empty())
      {
        rv = fs::clonepath(fromsrc,tosrc,dirname);
        if(rv == -1)
          return -1;
      }

    frompath = fs::path::make(fromsrc,relative);
    rv = fs::stat(frompath,&st);
    if(rv == -1)
      return -1;
    else if(!S_ISDIR(st.st_mode))
      return (errno=ENOTDIR,-1);

    topath = fs::path::make(tosrc,relative);
    rv = fs::mkdir(topath,st.st_mode);
    if(rv == -1)
      {
        if(errno == EEXIST)
          return 0;
        else
          return -1;
      }

    // it may not support it... it's fine...
    rv = fs::attr::copy(frompath,topath);
    if(return_metadata_errors && (rv == -1) && !ignorable_error(errno))
      return -1;

    // it may not support it... it's fine...
    rv = fs::xattr::copy(frompath,topath);
    if(return_metadata_errors && (rv == -1) && !ignorable_error(errno))
      return -1;

    rv = fs::lchown_check_on_error(topath,st);
    if(return_metadata_errors && (rv == -1))
      return -1;

    rv = fs::utime(topath,st);
    if(return_metadata_errors && (rv == -1))
      return -1;

    return 0;
  }

  int
  clonepath(const string &from,
            const string &to,
            const string &relative,
            const bool    return_metadata_errors)
  {
    return fs::clonepath(from,to,relative.c_str(),return_metadata_errors);
  }

  int
  clonepath_as_root(const string &from,
                    const string &to,
                    const char   *relative,
                    const bool    return_metadata_errors)
  {
    if((relative == NULL) || (relative[0] == '\0'))
      return 0;
    if(from == to)
      return 0;

    {
      const ugid::SetRootGuard ugidGuard;

      return fs::clonepath(from,to,relative,return_metadata_errors);
    }
  }

  int
  clonepath_as_root(const string &from,
                    const string &to,
                    const string &relative,
                    const bool    return_metadata_errors)
  {
    return fs::clonepath_as_root(from,to,relative.c_str(),return_metadata_errors);
  }
}
