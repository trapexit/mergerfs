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

#include "fs_fstat.hpp"
#include "fs_copydata.hpp"
#include "fs_attr.hpp"
#include "fs_xattr.hpp"
#include "fs_fchown.hpp"
#include "fs_fchmod.hpp"
#include "fs_futimens.hpp"


namespace l
{
  static
  bool
  ignorable_error(const int err_)
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
}

namespace fs
{
  int
  clonefile(const int src_fd_,
            const int dst_fd_)
  {
    int rv;
    struct stat src_st;

    rv = fs::fstat(src_fd_,&src_st);
    if(rv == -1)
      return -1;

    rv = fs::copydata(src_fd_,dst_fd_,src_st.st_size);
    if(rv == -1)
      return -1;

    rv = fs::attr::copy(src_fd_,dst_fd_);
    if((rv == -1) && !l::ignorable_error(errno))
      return -1;

    rv = fs::xattr::copy(src_fd_,dst_fd_);
    if((rv == -1) && !l::ignorable_error(errno))
      return -1;

    rv = fs::fchown_check_on_error(dst_fd_,src_st);
    if(rv == -1)
      return -1;

    rv = fs::fchmod_check_on_error(dst_fd_,src_st);
    if(rv == -1)
      return -1;

    rv = fs::futimens(dst_fd_,src_st);
    if(rv == -1)
      return -1;

    return 0;
  }
}
