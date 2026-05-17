/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fs_close.hpp"
#include "fs_fstat.hpp"
#include "fs_mkdirat.hpp"
#include "fs_openat.hpp"
#include "fs_fchown.hpp"
#include "fs_fchmod.hpp"
#include "fs_futimens.hpp"

#include "scope_guard/scope_guard.hpp"


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

  Iterative root-to-leaf walk. Stack usage is O(1) in component count.
*/
int
fs::clonepath(const std::string_view srcpath_,
              const std::string_view dstpath_,
              const fs::relpath     &relpath_,
              const bool             return_metadata_errors_)
{
  if(relpath_.empty())
    return 0;

  std::string srcpath;
  std::string dstpath;
  srcpath.reserve(srcpath_.size() + relpath_.size() + 1);
  dstpath.reserve(dstpath_.size() + relpath_.size() + 1);
  srcpath = srcpath_;
  dstpath = dstpath_;

  std::string_view rel = relpath_.native();
  std::size_t      pos = 0;
  while(pos < rel.size())
    {
      std::size_t next = rel.find('/',pos);
      if(next == std::string_view::npos)
        next = rel.size();

      std::string_view component = rel.substr(pos,next - pos);
      pos = next + 1;
      // Defensive: canonical rel form should not produce empty
      // segments, but skip them if the input is malformed (e.g. a
      // double slash) rather than appending nothing and re-stat'ing
      // the same path.
      if(component.empty())
        continue;

      srcpath += '/';
      srcpath.append(component);
      dstpath += '/';
      dstpath.append(component);

      int         rv;
      struct stat st;

      rv = fs::lstat(srcpath,&st);
      if(rv < 0)
        return rv;
      if(!S_ISDIR(st.st_mode))
        return -ENOTDIR;

      rv = fs::mkdir(dstpath,st.st_mode);
      if(rv < 0 && rv != -EEXIST)
        return rv;
      // EEXIST: directory already there (could be from a prior clone
      // or a race). Leave it alone, do NOT re-copy metadata, matching
      // the recursive version's behavior when its mkdir hit EEXIST.
      if(rv == -EEXIST)
        continue;

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
    }

  return 0;
}


// WORK IN PROGRESS
static
int
_clonepath2(const int          srcfd_,
            const int          dstfd_,
            const fs::relpath &dirname_,
            const bool         return_metadata_errors_)
{
  int rv;
  int srcdirfd;
  int dstdirfd;
  struct stat st;

  if(dirname_.empty())
    return 0;

  rv = fs::mkdirat(dstfd_,dirname_,0);
  if(rv < 0)
    return ((rv == -EEXIST) ? 0 : rv);

  srcdirfd = fs::openat(srcfd_,dirname_,O_DIRECTORY);
  if(srcdirfd < 0)
    return srcdirfd;
  DEFER { fs::close(srcdirfd); };

  dstdirfd = fs::openat(dstfd_,dirname_,O_DIRECTORY);
  if(dstdirfd < 0)
    return dstdirfd;
  DEFER { fs::close(dstdirfd); };

  rv = fs::attr::copy(srcdirfd,dstdirfd,FS_ATTR_CLEAR_IMMUTABLE);
  if(return_metadata_errors_ && (rv < 0) && !::_ignorable_error(-rv))
    return rv;

  rv = fs::xattr::copy(srcdirfd,dstdirfd);
  if(return_metadata_errors_ && (rv < 0) && !::_ignorable_error(-rv))
    return rv;

  rv = fs::fstat(srcdirfd,&st);
  if(rv < 0)
    return rv;
  if(!S_ISDIR(st.st_mode))
    return -ENOTDIR;

  rv = fs::fchown_check_on_error(dstdirfd,st);
  if(rv < 0)
    return rv;

  rv = fs::fchmod_check_on_error(dstdirfd,st);
  if(rv < 0)
    return rv;

  rv = fs::futimens(dstdirfd,st);
  if(rv < 0)
    return rv;

  return 0;
}
