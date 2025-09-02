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

#include "fs_clonepath.hpp"

#include "errno.h"
#include "fs_attr.hpp"
#include "fs_clonepath.hpp"
#include "fs_lchown.hpp"
#include "fs_lstat.hpp"
#include "fs_lutimens.hpp"
#include "fs_mkdir.hpp"
#include "fs_path.hpp"
#include "fs_xattr.hpp"
#include "ugid.hpp"

#include <filesystem>
#include <string>

using std::string;
namespace stdfs = std::filesystem;

static
bool
_ignorable_error(const int err_)
{
  switch(err_)
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

/*
  Attempts to clone a path.
  The directories which already exist are left alone.
  The new directories have metadata set to match the original if
  possible. Optionally ignore errors on metadata copies.
*/
int
fs::clonepath(const string &srcpath_,
              const string &dstpath_,
              const char   *relpath_,
              const bool    return_metadata_errors_)
{
  int         rv;
  struct stat st;
  string      dstpath;
  string      srcpath;
  stdfs::path dirname;
  stdfs::path relpath;

  if((relpath_ == NULL) || (relpath_[0] == '\0'))
    return 0;

  relpath = relpath_;
  if(relpath_[0] != '/')
    relpath = '/' + relpath;

  dirname = relpath.parent_path();
  if(dirname != dirname.root_path())
    {
      rv = fs::clonepath(srcpath_,dstpath_,dirname,return_metadata_errors_);
      if(rv < 0)
        return rv;
    }

  srcpath = fs::path::make(srcpath_,relpath_);
  rv = fs::lstat(srcpath,&st);
  if(rv < 0)
    return rv;
  else if(!S_ISDIR(st.st_mode))
    return -ENOTDIR;

  dstpath = fs::path::make(dstpath_,relpath_);
  rv = fs::mkdir(dstpath,st.st_mode);
  if(rv < 0)
    return ((rv == -EEXIST) ? 0 : rv);

  // it may not support it... it's fine...
  rv = fs::attr::copy(srcpath,dstpath);
  if(return_metadata_errors_ && (rv < 0) && !::_ignorable_error(-rv))
    return rv;

  // it may not support it... it's fine...
  rv = fs::xattr::copy(srcpath,dstpath);
  if(return_metadata_errors_ && (rv < 0) && !::_ignorable_error(-rv))
    return rv;

  rv = fs::lchown_check_on_error(dstpath,st);
  if(return_metadata_errors_ && (rv < 0))
    return rv;

  rv = fs::lutimens(dstpath,st);
  if(return_metadata_errors_ && (rv < 0))
    return rv;

  return 0;
}

int
fs::clonepath(const string &srcpath_,
              const string &dstpath_,
              const string &relpath_,
              const bool    return_metadata_errors_)
{
  return fs::clonepath(srcpath_,
                       dstpath_,
                       relpath_.c_str(),
                       return_metadata_errors_);
}

int
fs::clonepath_as_root(const string &srcpath_,
                      const string &dstpath_,
                      const char   *relpath_,
                      const bool    return_metadata_errors_)
{
  if((relpath_ == NULL) || (relpath_[0] == '\0'))
    return 0;
  if(srcpath_ == dstpath_)
    return 0;

  const ugid::SetRootGuard ugid_guard;

  return fs::clonepath(srcpath_,
                       dstpath_,
                       relpath_,
                       return_metadata_errors_);
}

int
fs::clonepath_as_root(const string &srcpath_,
                      const string &dstpath_,
                      const string &relpath_,
                      const bool    return_metadata_errors_)
{
  return fs::clonepath_as_root(srcpath_,
                               dstpath_,
                               relpath_.c_str(),
                               return_metadata_errors_);
}
