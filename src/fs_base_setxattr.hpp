/*
  ISC License

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

#ifndef __FS_BASE_SETXATTR_HPP__
#define __FS_BASE_SETXATTR_HPP__

#include <sys/types.h>

#include "errno.hpp"
#include "xattr.hpp"

namespace fs
{
  static
  inline
  int
  lsetxattr(const std::string &path,
            const char        *name,
            const void        *value,
            const size_t       size,
            const int          flags,
            const u_int32_t    position)
  {
  #if WITHOUT_XATTR
    return (errno=ENOTSUP,-1);
  #elif __APPLE__
    return ::setxattr(path.c_str(),name,value,size,position,flags & XATTR_NOFOLLOW);
  #else
    return ::lsetxattr(path.c_str(),name,value,size,flags);
  #endif
  }
}

#endif
