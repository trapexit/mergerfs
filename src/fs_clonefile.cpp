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

#include <fcntl.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "errno.hpp"
#include "fs_attr.hpp"
#include "fs_base_chmod.hpp"
#include "fs_base_chown.hpp"
#include "fs_base_close.hpp"
#include "fs_base_lseek.hpp"
#include "fs_base_mkdir.hpp"
#include "fs_base_open.hpp"
#include "fs_base_read.hpp"
#include "fs_base_stat.hpp"
#include "fs_base_utime.hpp"
#include "fs_base_write.hpp"
#include "fs_fadvise.hpp"
#include "fs_fallocate.hpp"
#include "fs_sendfile.hpp"
#include "fs_xattr.hpp"

using std::string;
using std::vector;

int
writen(const int     fd,
       const char   *buf,
       const size_t  count)
{
  size_t nleft;
  ssize_t nwritten;

  nleft = count;
  while(nleft > 0)
    {
      nwritten = fs::write(fd,buf,nleft);
      if(nwritten == -1)
        {
          if(errno == EINTR)
            continue;
          return -1;
        }

      nleft -= nwritten;
      buf   += nwritten;
    }

  return count;
}

static
int
copyfile_rw(const int    fdin,
            const int    fdout,
            const size_t count,
            const size_t blocksize)
{
  ssize_t nr;
  ssize_t nw;
  ssize_t bufsize;
  size_t  totalwritten;
  vector<char> buf;

  bufsize = (blocksize * 16);
  buf.resize(bufsize);

  fs::lseek(fdin,0,SEEK_SET);

  totalwritten = 0;
  while(totalwritten < count)
    {
      nr = fs::read(fdin,&buf[0],bufsize);
      if(nr == -1)
        {
          if(errno == EINTR)
            continue;
          return -1;
        }

      nw = writen(fdout,&buf[0],nr);
      if(nw == -1)
        return -1;

      totalwritten += nw;
    }

  return count;
}

static
int
copydata(const int    fdin,
         const int    fdout,
         const size_t count,
         const size_t blocksize)
{
  int rv;

  fs::fadvise(fdin,0,count,POSIX_FADV_WILLNEED);
  fs::fadvise(fdin,0,count,POSIX_FADV_SEQUENTIAL);

  fs::fallocate(fdout,0,0,count);

  rv = fs::sendfile(fdin,fdout,count);
  if((rv == -1) && ((errno == EINVAL) || (errno == ENOSYS)))
    return ::copyfile_rw(fdin,fdout,count,blocksize);

  return rv;
}

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
  int
  clonefile(const int fdin,
            const int fdout)
  {
    int rv;
    struct stat stin;

    rv = fs::fstat(fdin,stin);
    if(rv == -1)
      return -1;

    rv = ::copydata(fdin,fdout,stin.st_size,stin.st_blksize);
    if(rv == -1)
      return -1;

    rv = fs::attr::copy(fdin,fdout);
    if((rv == -1) && !ignorable_error(errno))
      return -1;

    rv = fs::xattr::copy(fdin,fdout);
    if((rv == -1) && !ignorable_error(errno))
      return -1;

    rv = fs::fchown(fdout,stin);
    if(rv == -1)
      return -1;

    rv = fs::fchmod(fdout,stin);
    if(rv == -1)
      return -1;

    rv = fs::utime(fdout,stin);
    if(rv == -1)
      return -1;

    return 0;
  }

  int
  clonefile(const string &in,
            const string &out)
  {
    int rv;
    int fdin;
    int fdout;
    int error;

    fdin = fs::open(in,O_RDONLY|O_NOFOLLOW);
    if(fdin == -1)
      return -1;

    const int    flags = O_CREAT|O_LARGEFILE|O_NOATIME|O_NOFOLLOW|O_TRUNC|O_WRONLY;
    const mode_t mode  = S_IWUSR;
    fdout = fs::open(out,flags,mode);
    if(fdout == -1)
      return -1;

    rv = fs::clonefile(fdin,fdout);
    error = errno;

    fs::close(fdin);
    fs::close(fdout);

    errno = error;
    return rv;
  }
}
