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

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_close.hpp"
#include "fs_fstat.hpp"
#include "fs_inode.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "fs_read.hpp"
#include "ugid.hpp"

#include <fuse.h>

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
  failover_fstat_loop(FileInfo        *fi_,
                      const BranchVec &branches_,
                      struct stat     *st_,
                      const int        error_)
  {
    int fd;
    int rv;
    char buf;
    std::string fullpath;

    for(size_t i = 0, ei = branches_.size(); i != ei; i++)
      {
        fullpath = fs::path::make(branches_[i].path,fi_->fusepath);

        fd = fs::open(fullpath,fi_->flags);
        if(fd == -1)
          continue;

        rv = fs::pread(fd,&buf,1,0);
        if(rv == -1)
          {
            fs::close(fd);
            continue;
          }

        rv = fs::fstat(fd,st_);
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
  failover_fstat(const Branches &branches_,
                 FileInfo       *fi_,
                 struct stat    *st_,
                 const int       error_)
  {
    const fuse_context *fc = fuse_get_context();
    const Config &config = Config::ro();
    const ugid::Set ugid(fc->uid,fc->gid);
    rwlock::ReadGuard guard(config.branches.lock);

    return l::failover_fstat_loop(fi_,config.branches.vec,st_,error_);
  }

  static
  int
  maybe_failover_fstat(FileInfo    *fi_,
                       struct stat *st_,
                       const int    error_)
  {
    const Config &config = Config::ro();

    if(config.readfailover == false)
      return (errno=error_,-1);
    if(l::failoverable_flags(fi_->flags) == false)
      return (errno=error_,-1);
    if(l::failoverable_error(error_) == false)
      return (errno=error_,-1);

    return l::failover_fstat(config.branches,fi_,st_,error_);
  }

  static
  int
  fgetattr(FileInfo    *fi_,
           struct stat *st_)
  {
    int rv;

    rv = fs::fstat(fi_->fd,st_);
    if(rv == -1)
      rv = l::maybe_failover_fstat(fi_,st_,errno);

    if(rv == -1)
      return -errno;

    fs::inode::calc(fi_->fusepath,st_);

    return 0;
  }
}

namespace FUSE
{
  int
  fgetattr(struct stat     *st_,
           fuse_file_info  *ffi_,
           fuse_timeouts_t *timeout_)
  {
    int rv;
    const Config &config = Config::ro();
    FileInfo *fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    rv = l::fgetattr(fi,st_);

    timeout_->entry = ((rv >= 0) ?
                       config.cache_entry :
                       config.cache_negative_entry);
    timeout_->attr  = config.cache_attr;

    return rv;
  }
}
