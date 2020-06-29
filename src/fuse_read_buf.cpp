/*
  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct fuse_bufvec fuse_bufvec;

namespace l
{
  static
  bool
  failoverable_error(const int error_)
  {
    switch(error_)
      {
      case EIO:
      case ENOTCONN:
        return true;
      default:
        return false;
      }
  }

  static
  bool
  failoverable_flags(const int flags_)
  {
    return !(flags_ & (O_CREAT|O_TRUNC));
  }

  static
  void
  update_fi_fd(FileInfo  *fi_,
               const int  fd_)
  {
    int tmp;

    tmp = fi_->fd;
    fi_->fd = fd_;
    fs::close(tmp);
  }

  static
  int
  failover_read_loop(const BranchVec &branches_,
                     FileInfo        *fi_,
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
        if(rv == -1)
          {
            fs::close(fd);
            continue;
          }

        l::update_fi_fd(fi_,fd);

        return rv;
      }

    return (errno=error_,-1);
  }

  static
  int
  failover_read(const Branches &branches_,
                FileInfo       *fi_,
                void           *buf_,
                const size_t    count_,
                const off_t     offset_,
                const int       error_)
  {
    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);
    rwlock::ReadGuard   rwlock_guard(branches_.lock);

    return l::failover_read_loop(branches_.vec,fi_,buf_,count_,offset_,error_);
  }

  static
  int
  maybe_failover_read(FileInfo     *fi_,
                      void         *buf_,
                      const size_t  count_,
                      const off_t   offset_,
                      const int     error_)
  {
    const Config &config = Config::ro();

    if(config.readfailover == false)
      return (errno=error_,-1);
    if(l::failoverable_flags(fi_->flags) == false)
      return (errno=error_,-1);
    if(l::failoverable_error(error_) == false)
      return (errno=error_,-1);

    return l::failover_read(config.branches,fi_,buf_,count_,offset_,error_);
  }

  static
  int
  read_buf_null(fuse_bufvec  **bufp_,
                const size_t   count_,
                const off_t    offset_)
  {
    void *buf;
    fuse_bufvec *src;

    buf = (void*)calloc(count_,1);
    if(buf == NULL)
      return -ENOMEM;

    src = (fuse_bufvec*)malloc(sizeof(fuse_bufvec));
    if(src == NULL)
      {
        free(buf);
        return -ENOMEM;
      }

    *src = FUSE_BUFVEC_INIT(count_);

    src->buf->mem  = buf;
    src->buf->size = count_;

    *bufp_ = src;

    return 0;
  }

  static
  int
  read_buf(FileInfo      *fi_,
           fuse_bufvec  **bufp_,
           const size_t   count_,
           const off_t    offset_)
  {
    int rv;
    void *buf;
    fuse_bufvec *src;

    buf = (void*)malloc(count_);
    if(buf == NULL)
      return -ENOMEM;

    src = (fuse_bufvec*)malloc(sizeof(fuse_bufvec));
    if(src == NULL)
      {
        free(buf);
        return -ENOMEM;
      }

    *src = FUSE_BUFVEC_INIT(count_);

    rv = fs::pread(fi_->fd,buf,count_,offset_);
    if(rv == -1)
      rv = l::maybe_failover_read(fi_,buf,count_,offset_,errno);

    if(rv == -1)
      {
        free(buf);
        free(src);
        return -errno;
      }

    src->buf->mem  = buf;
    src->buf->size = rv;

    *bufp_ = src;

    return 0;
  }
}

namespace FUSE
{
  int
  read_buf_null(fuse_bufvec    **bufp_,
                size_t           count_,
                off_t            offset_,
                fuse_file_info  *ffi_)
  {
    return l::read_buf_null(bufp_,
                            count_,
                            offset_);
  }

  int
  read_buf(fuse_bufvec    **bufp_,
           size_t           count_,
           off_t            offset_,
           fuse_file_info  *ffi_)
  {
    FileInfo *fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    return l::read_buf(fi,
                       bufp_,
                       count_,
                       offset_);
  }
}
