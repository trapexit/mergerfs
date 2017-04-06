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

#ifndef O_LARGEFILE
# define O_LARGEFILE 0
#endif

#ifndef O_NOATIME
# define O_NOATIME 0
#endif

using std::string;
using std::vector;

ssize_t
writen(const int     fd,
       const char   *buf,
       const size_t  count)
{
  size_t nleft;
  ssize_t nwritten = 0;

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

      nleft -= (size_t)nwritten;
      buf   += (size_t)nwritten;
    }

  return nwritten;
}

static
ssize_t
copyfile_rw(const int    fdin,
            const int    fdout,
            const size_t count,
            const size_t blocksize)
{
  ssize_t nr;
  ssize_t nw;
  size_t  bufsize;
  ssize_t totalwritten;
  vector<char> buf;

  bufsize = (blocksize * 16);
  buf.resize(bufsize);

  fs::lseek(fdin,0,SEEK_SET);

  totalwritten = 0;
  while(totalwritten < (ssize_t)count)
    {
      nr = fs::read(fdin,&buf[0],bufsize);
      if(nr == -1)
        {
          if(errno == EINTR)
            continue;
          return -1;
        }

      nw = writen(fdout,&buf[0],(size_t)nr);
      if(nw == -1)
        return -1;

      totalwritten += nw;
    }

  return totalwritten;
}

static
ssize_t
copydata(const int    fdin,
         const int    fdout,
         const size_t count,
         const size_t blocksize)
{
  ssize_t rv;
  off_t scount = (off_t)count;
	
  fs::fadvise(fdin,0,scount,POSIX_FADV_WILLNEED);
  fs::fadvise(fdin,0,scount,POSIX_FADV_SEQUENTIAL);

  fs::fallocate(fdout,0,0,scount);

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
  ssize_t
  clonefile(const int fdin,
            const int fdout)
  {
    ssize_t rv;
    struct stat stin;

    rv = fs::fstat(fdin,stin);
    if(rv == -1)
      return -1;

    rv = ::copydata(fdin,fdout,(size_t)stin.st_size,(size_t)stin.st_blksize);
    if(rv == -1)
      return -1;

    rv = fs::attr::copy(fdin,fdout);
    if((rv == -1) && !ignorable_error(errno))
      return -1;

    rv = fs::xattr::copy(fdin,fdout);
    if((rv == -1) && !ignorable_error(errno))
      return -1;

    rv = fs::fchown_check_on_error(fdout,stin);
    if(rv == -1)
      return -1;

    rv = fs::fchmod_check_on_error(fdout,stin);
    if(rv == -1)
      return -1;

    rv = fs::utime(fdout,stin);
    if(rv == -1)
      return -1;

    return 0;
  }

  ssize_t
  clonefile(const string &in,
            const string &out)
  {
    ssize_t rv;
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
