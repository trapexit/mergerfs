/*
  The MIT License (MIT)

  Copyright (c) 2015 Antonio SJ Musumeci <trapexit@spawn.link>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#ifdef __linux__
#include <sys/sendfile.h>
#endif

#include <string>
#include <vector>

#include "fs_attr.hpp"
#include "fs_xattr.hpp"

using std::string;
using std::vector;

namespace fs
{
  static
  ssize_t
  sendfile(const int    fdin,
           const int    fdout,
           const size_t count)
  {
#if defined __linux__
    off_t offset = 0;
    return ::sendfile(fdout,fdin,&offset,count);
#else
    return (errno=EINVAL,-1);
#endif
  }

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
        nwritten = ::write(fd,buf,nleft);
        if(nwritten == -1)
          {
            if(errno == EINTR)
              continue;
            return -1;
          }

        nleft -= nwritten;
        buf += nwritten;
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

    totalwritten = 0;
    while(totalwritten < count)
      {
        nr = ::read(fdin,&buf[0],bufsize);
        if(nr == -1)
          {
            if(errno == EINTR)
              continue;
            else
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

    ::posix_fadvise(fdin,0,count,POSIX_FADV_WILLNEED);
    ::posix_fadvise(fdin,0,count,POSIX_FADV_SEQUENTIAL);

    ::posix_fallocate(fdout,0,count);

    rv = fs::sendfile(fdin,fdout,count);
    if((rv == -1) && ((errno == EINVAL) || (errno == ENOSYS)))
      return fs::copyfile_rw(fdin,fdout,count,blocksize);

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

  int
  clonefile(const int fdin,
            const int fdout)
  {
    int rv;
    struct stat stin;

    rv = ::fstat(fdin,&stin);
    if(rv == -1)
      return -1;

    rv = copydata(fdin,fdout,stin.st_size,stin.st_blksize);
    if(rv == -1)
      return -1;

    rv = fs::attr::copy(fdin,fdout);
    if(rv == -1 && !ignorable_error(errno))
      return -1;

    rv = fs::xattr::copy(fdin,fdout);
    if(rv == -1 && !ignorable_error(errno))
      return -1;

    rv = ::fchown(fdout,stin.st_uid,stin.st_gid);
    if(rv == -1)
      return -1;

    rv = ::fchmod(fdout,stin.st_mode);
    if(rv == -1)
      return -1;

    struct timespec times[2];
    times[0] = stin.st_atim;
    times[1] = stin.st_mtim;
    rv = ::futimens(fdout,times);
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

    fdin = ::open(in.c_str(),O_RDONLY|O_NOFOLLOW);
    if(fdin == -1)
      return -1;

    const int flags = O_CREAT|O_LARGEFILE|O_NOATIME|O_NOFOLLOW|O_TRUNC|O_WRONLY;
    const int mode  = S_IWUSR;
    fdout = ::open(out.c_str(),flags,mode);
    if(fdout == -1)
      return -1;

    rv = fs::clonefile(fdin,fdout);
    error = errno;

    ::close(fdin);
    ::close(fdout);

    errno = error;
    return rv;
  }
}
