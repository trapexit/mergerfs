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

#include "errno.hpp"
#include "fs_attr.hpp"
#include "fs_base_chmod.hpp"
#include "fs_base_chown.hpp"
#include "fs_base_fadvise.hpp"
#include "fs_base_fallocate.hpp"
#include "fs_base_fchmod.hpp"
#include "fs_base_fchown.hpp"
#include "fs_base_ftruncate.hpp"
#include "fs_base_stat.hpp"
#include "fs_base_utime.hpp"
#include "fs_copy_file_range.hpp"
#include "fs_copyfile.hpp"
#include "fs_ficlone.hpp"
#include "fs_sendfile.hpp"
#include "fs_xattr.hpp"

static
int
copydata(const int    src_fd_,
         const int    dst_fd_,
         const size_t count_)
{
  int rv;

  rv = fs::ftruncate(dst_fd_,count_);
  if(rv == -1)
    return -1;

  rv = fs::ficlone(src_fd_,dst_fd_);
  if(rv != -1)
    return rv;

  fs::fadvise_willneed(src_fd_,0,count_);
  fs::fadvise_sequential(src_fd_,0,count_);

  rv = fs::copy_file_range(src_fd_,dst_fd_,count_);
  if(rv != -1)
    return rv;

  rv = fs::sendfile(src_fd_,dst_fd_,count_);
  if(rv != -1)
    return rv;

  return fs::copyfile(src_fd_,dst_fd_);
}

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

    rv = ::copydata(src_fd_,dst_fd_,src_st.st_size);
    if(rv == -1)
      return -1;

    rv = fs::attr::copy(src_fd_,dst_fd_);
    if((rv == -1) && !ignorable_error(errno))
      return -1;

    rv = fs::xattr::copy(src_fd_,dst_fd_);
    if((rv == -1) && !ignorable_error(errno))
      return -1;

    rv = fs::fchown_check_on_error(dst_fd_,src_st);
    if(rv == -1)
      return -1;

    rv = fs::fchmod_check_on_error(dst_fd_,src_st);
    if(rv == -1)
      return -1;

    rv = fs::futime(dst_fd_,src_st);
    if(rv == -1)
      return -1;

    return 0;
  }
}
