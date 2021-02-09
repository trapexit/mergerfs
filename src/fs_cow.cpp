/*
  ISC License

  Copyright (c) 2018, Antonio SJ Musumeci <trapexit@spawn.link>

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
#include "fs_clonefile.hpp"
#include "fs_close.hpp"
#include "fs_lstat.hpp"
#include "fs_mktemp.hpp"
#include "fs_open.hpp"
#include "fs_rename.hpp"
#include "fs_unlink.hpp"

#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using std::string;


namespace l
{
  static
  int
  cleanup_on_error(const int     src_fd_,
                   const int     dst_fd_       = -1,
                   const string &dst_fullpath_ = string())
  {
    int error = errno;

    if(src_fd_ >= 0)
      fs::close(src_fd_);
    if(dst_fd_ >= 0)
      fs::close(dst_fd_);
    if(!dst_fullpath_.empty())
      fs::unlink(dst_fullpath_);

    errno = error;

    return -1;
  }
}

namespace fs
{
  namespace cow
  {
    bool
    is_eligible(const int flags_)
    {
      int accmode;

      accmode = (flags_ & O_ACCMODE);

      return ((accmode == O_RDWR) ||
              (accmode == O_WRONLY));
    }

    bool
    is_eligible(const struct stat &st_)
    {
      return ((S_ISREG(st_.st_mode)) && (st_.st_nlink > 1));
    }

    bool
    is_eligible(const int          flags_,
                const struct stat &st_)
    {
      return (is_eligible(flags_) && is_eligible(st_));
    }

    bool
    is_eligible(const char *fullpath_,
                const int   flags_)
    {
      int rv;
      struct stat st;

      if(!fs::cow::is_eligible(flags_))
        return false;

      rv = fs::lstat(fullpath_,&st);
      if(rv == -1)
        return false;

      return fs::cow::is_eligible(st);
    }

    int
    break_link(const char *src_fullpath_)
    {
      int rv;
      int src_fd;
      int dst_fd;
      string dst_fullpath;

      src_fd = fs::open(src_fullpath_,O_RDONLY|O_NOFOLLOW);
      if(src_fd == -1)
        return -1;

      dst_fullpath = src_fullpath_;

      dst_fd = fs::mktemp(&dst_fullpath,O_WRONLY);
      if(dst_fd == -1)
        return l::cleanup_on_error(src_fd);

      rv = fs::clonefile(src_fd,dst_fd);
      if(rv == -1)
        return l::cleanup_on_error(src_fd,dst_fd,dst_fullpath);

      rv = fs::rename(dst_fullpath,src_fullpath_);
      if(rv == -1)
        return l::cleanup_on_error(src_fd,dst_fd,dst_fullpath);

      fs::close(src_fd);
      fs::close(dst_fd);

      return 0;
    }
  }
}
