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

#include "branch.hpp"
#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_close.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "fs_read.hpp"
#include "ugid.hpp"

#include <fuse.h>

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace l
{
  static
  bool
  can_failover(const int error_,
               const int flags_)
  {
    return (((error_ == EIO) ||
             (error_ == ENOTCONN)) &&
            !(flags_ & (O_CREAT|O_TRUNC)));
  }

  static
  int
  failover_read_loop(FileInfo        *fi_,
                     const BranchVec &branches_,
                     void            *buf_,
                     const size_t     count_,
                     const off_t      offset_,
                     const int        error_)
  {
    int fd;
    int rv;
    std::string fullpath;

    for(size_t i = 0, ei = branches_.size(); i != ei; i++)
      {
        fullpath = fs::path::make(branches_[i].path,fi_->fusepath);

        fd = fs::open(fullpath,fi_->flags);
        if(fd == -1)
          continue;

        rv = fs::pread(fd,buf_,count_,offset_);
        if(rv >= 0)
          {
            fs::close(fi_->fd);
            fi_->fd = fd;
            return rv;
          }

        fs::close(fd);
      }

    return (errno=error_,-1);
  }

  static
  int
  failover_read(FileInfo     *fi_,
                void         *buf_,
                const size_t  count_,
                const off_t   offset_,
                const int     error_)
  {
    const fuse_context *fc     = fuse_get_context();
    const Config       &config = Config::ro();
    const ugid::Set     ugid(fc->uid,fc->gid);
    rwlock::ReadGuard   guard(config.branches.lock);

    return l::failover_read_loop(fi_,config.branches.vec,buf_,count_,offset_,error_);
  }

  static
  inline
  int
  read_regular(FileInfo     *fi_,
               void         *buf_,
               const size_t  count_,
               const off_t   offset_)
  {
    int rv;

    rv = fs::pread(fi_->fd,buf_,count_,offset_);
    if((rv == -1) && l::can_failover(errno,fi_->flags))
      rv = l::failover_read(fi_,buf_,count_,offset_,errno);

    if(rv == -1)
      return -errno;
    if(rv == 0)
      return 0;

    return count_;
  }

  static
  inline
  int
  read_direct_io(FileInfo     *fi_,
                 void         *buf_,
                 const size_t  count_,
                 const off_t   offset_)
  {
    int rv;

    rv = fs::pread(fi_->fd,buf_,count_,offset_);
    if((rv == -1) && l::can_failover(errno,fi_->flags))
      rv = l::failover_read(fi_,buf_,count_,offset_,errno);

    if(rv == -1)
      return -errno;

    return rv;
  }
}

namespace FUSE
{
  int
  read(char           *buf_,
       size_t          count_,
       off_t           offset_,
       fuse_file_info *ffi_)
  {
    FileInfo *fi;

    fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    if(ffi_->direct_io)
      return l::read_direct_io(fi,buf_,count_,offset_);
    return l::read_regular(fi,buf_,count_,offset_);
  }

  int
  read_null(char           *buf_,
            size_t          count_,
            off_t           offset_,
            fuse_file_info *ffi_)

  {
    return count_;
  }
}
