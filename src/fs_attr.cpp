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

#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

using std::string;

namespace fs
{
  namespace attr
  {
    static
    int
    get_fs_ioc_flags(const int  fd,
                     int       &flags)
    {
      return ::ioctl(fd,FS_IOC_GETFLAGS,&flags);
    }

    static
    int
    get_fs_ioc_flags(const string &file,
                     int          &flags)
    {
      int fd;
      int rv;
      const int openflags = O_RDONLY|O_NONBLOCK;

      fd = ::open(file.c_str(),openflags);
      if(fd == -1)
        return -1;

      rv = get_fs_ioc_flags(fd,flags);
      if(rv == -1)
        {
          int error = errno;
          ::close(fd);
          errno = error;
          return -1;
        }

      return ::close(fd);
    }

    static
    int
    set_fs_ioc_flags(const int fd,
                     const int flags)
    {
      return ::ioctl(fd,FS_IOC_SETFLAGS,&flags);
    }

    static
    int
    set_fs_ioc_flags(const string &file,
                     const int     flags)
    {
      int fd;
      int rv;
      const int openflags = O_RDONLY|O_NONBLOCK;

      fd = ::open(file.c_str(),openflags);
      if(fd == -1)
        return -1;

      rv = set_fs_ioc_flags(fd,flags);
      if(rv == -1)
        {
          int error = errno;
          ::close(fd);
          errno = error;
          return -1;
        }

      return ::close(fd);
    }

    int
    copy(const int fdin,
         const int fdout)
    {
      int rv;
      int flags;

      rv = get_fs_ioc_flags(fdin,flags);
      if(rv == -1)
        return -1;

      return set_fs_ioc_flags(fdout,flags);
    }

    int
    copy(const string &from,
         const string &to)
    {
      int rv;
      int flags;

      rv = get_fs_ioc_flags(from,flags);
      if(rv == -1)
        return -1;

      return set_fs_ioc_flags(to,flags);
    }
  }
}
