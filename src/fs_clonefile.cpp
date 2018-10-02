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
#include "fs_base_close.hpp"
#include "fs_base_fadvise.hpp"
#include "fs_base_fallocate.hpp"
#include "fs_base_lseek.hpp"
#include "fs_base_mkdir.hpp"
#include "fs_base_open.hpp"
#include "fs_base_read.hpp"
#include "fs_base_stat.hpp"
#include "fs_base_utime.hpp"
#include "fs_base_write.hpp"
#include "fs_sendfile.hpp"
#include "fs_xattr.hpp"

#include <fcntl.h>
#include <stdlib.h>

#include <string>
#include <vector>

#ifndef O_LARGEFILE
# define O_LARGEFILE 0
#endif

#ifndef O_NOATIME
# define O_NOATIME 0
#endif

using std::string;
using std::vector;

int
writen(const int     fd_,
       const char   *buf_,
       const size_t  count_)
{
  size_t nleft;
  ssize_t nwritten;

  nleft = count_;
  do
    {
      nwritten = fs::write(fd_,buf_,nleft);
      if((nwritten == -1) && (errno == EINTR))
        continue;
      if(nwritten == -1)
        return -1;

      nleft -= nwritten;
      buf_  += nwritten;
    }
  while(nleft > 0);

  return count_;
}

static
int
copyfile_rw(const int    src_fd_,
            const int    dst_fd_,
            const size_t count_,
            const size_t blocksize_)
{
  ssize_t nr;
  ssize_t nw;
  ssize_t bufsize;
  size_t  totalwritten;
  vector<char> buf;

  bufsize = (blocksize_ * 16);
  buf.resize(bufsize);

  fs::lseek(src_fd_,0,SEEK_SET);

  totalwritten = 0;
  while(totalwritten < count_)
    {
      nr = fs::read(src_fd_,&buf[0],bufsize);
      if(nr == 0)
        return totalwritten;
      if((nr == -1) && (errno == EINTR))
        continue;
      if(nr == -1)
        return -1;

      nw = writen(dst_fd_,&buf[0],nr);
      if(nw == -1)
        return -1;

      totalwritten += nw;
    }

  return totalwritten;
}

static
int
copydata(const int    src_fd_,
         const int    dst_fd_,
         const size_t count_,
         const size_t blocksize_)
{
  int rv;

  fs::fadvise_willneed(src_fd_,0,count_);
  fs::fadvise_sequential(src_fd_,0,count_);

  fs::fallocate(dst_fd_,0,0,count_);

  rv = fs::sendfile(src_fd_,dst_fd_,count_);
  if((rv == -1) && ((errno == EINVAL) || (errno == ENOSYS)))
    return ::copyfile_rw(src_fd_,dst_fd_,count_,blocksize_);

  return rv;
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

    rv = fs::fstat(src_fd_,src_st);
    if(rv == -1)
      return -1;

    rv = ::copydata(src_fd_,dst_fd_,src_st.st_size,src_st.st_blksize);
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

    rv = fs::utime(dst_fd_,src_st);
    if(rv == -1)
      return -1;

    return 0;
  }
}
