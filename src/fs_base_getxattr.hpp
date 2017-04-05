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

#ifndef __FS_BASE_GETXATTR_HPP__
#define __FS_BASE_GETXATTR_HPP__

#include <sys/types.h>

#include "errno.hpp"
#include "xattr.hpp"

namespace fs
{
  static
  inline
  ssize_t
  lgetxattr(const std::string &path,
            const char        *attrname,
            void              *value,
            const size_t       size)
  {
#ifndef WITHOUT_XATTR
#if __APPLE__
    return ::getxattr(path.c_str(),attrname,value,size,0,XATTR_NOFOLLOW);
#else
    return ::lgetxattr(path.c_str(),attrname,value,size);
#endif /* __APPLE__ */
#else
    return (errno=ENOTSUP,-1);
#endif
  }
}

#endif
