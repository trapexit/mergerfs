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
#include "fs_dup2.hpp"
#include "fs_movefile.hpp"
#include "fs_pwrite.hpp"
#include "fs_pwriten.hpp"

#include "fuse.h"


namespace l
{
  static
  bool
  out_of_space(const ssize_t error_)
  {
    return ((error_ == -ENOSPC) ||
            (error_ == -EDQUOT));
  }

  static
  int
  move_and_pwrite(const char   *buf_,
                  const size_t  count_,
                  const off_t   offset_,
                  FileInfo     *fi_,
                  int           err_)
  {
    int err;
    ssize_t rv;
    Config::Read cfg;

    if(cfg->moveonenospc.enabled == false)
      return err_;

    rv = fs::movefile_as_root(cfg->moveonenospc.policy,
                              cfg->branches,
                              fi_->fusepath,
                              fi_->fd);
    if(rv < 0)
      return err_;

    err = fs::dup2(rv,fi_->fd);
    fs::close(rv);
    if(err < 0)
      return err_;

    return fs::pwrite(fi_->fd,buf_,count_,offset_);
  }

  static
  int
  move_and_pwriten(char const    *buf_,
                   size_t const   count_,
                   off_t const    offset_,
                   FileInfo      *fi_,
                   ssize_t const  err_,
                   ssize_t const  written_)
  {
    int err;
    ssize_t rv;
    Config::Read cfg;

    if(cfg->moveonenospc.enabled == false)
      return err_;

    rv = fs::movefile_as_root(cfg->moveonenospc.policy,
                              cfg->branches,
                              fi_->fusepath,
                              fi_->fd);
    if(rv < 0)
      return err_;

    err = fs::dup2(rv,fi_->fd);
    fs::close(rv);
    if(err < 0)
      return err_;

    rv = fs::pwriten(fi_->fd,
                     buf_ + written_,
                     count_ - written_,
                     offset_ + written_,
                     &err);
    if(err < 0)
      return err;

    return (written_ + rv);
  }

  // When in direct_io mode write's return value should match that of
  // the operation.
  // 0 on EOF
  // N bytes written (short writes included)
  // -errno on error
  // See libfuse/include/fuse.h for more details
  static
  int
  write_direct_io(const char   *buf_,
                  const size_t  count_,
                  const off_t   offset_,
                  FileInfo     *fi_)
  {
    ssize_t rv;

    rv = fs::pwrite(fi_->fd,buf_,count_,offset_);
    if(l::out_of_space(rv))
      rv = l::move_and_pwrite(buf_,count_,offset_,fi_,rv);

    return rv;
  }

  // When not in direct_io mode write's return value is more complex.
  // 0 or less than `count` on EOF
  // `count` on non-errors
  // -errno on error
  // See libfuse/include/fuse.h for more details
  static
  int
  write_cached(const char   *buf_,
               const size_t  count_,
               const off_t   offset_,
               FileInfo     *fi_)
  {
    int err;
    ssize_t rv;

    rv = fs::pwriten(fi_->fd,buf_,count_,offset_,&err);
    if(err == 0)
      return rv;
    if(err && !l::out_of_space(err))
      return err;

    rv = l::move_and_pwriten(buf_,count_,offset_,fi_,err,rv);

    return rv;
  }

  static
  int
  write(const fuse_file_info_t *ffi_,
        const char             *buf_,
        const size_t            count_,
        const off_t             offset_)
  {
    FileInfo *fi;

    fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    // Concurrent writes can only happen if:
    // 1) writeback-cache is enabled and using page caching
    // 2) parallel_direct_writes is enabled and file has `direct_io=true`
    // Will look into selectively locking in the future
    // A reader/writer lock would probably be the best option given
    // the expense of the write itself in comparison. Alternatively,
    // could change the move file behavior to use a known target file
    // and have threads use O_EXCL and back off and wait for the
    // transfer to complete before retrying.
    std::lock_guard<std::mutex> guard(fi->mutex);

    if(fi->direct_io)
      return l::write_direct_io(buf_,count_,offset_,fi);

    return l::write_cached(buf_,count_,offset_,fi);
  }
}

namespace FUSE
{
  int
  write(const fuse_file_info_t *ffi_,
        const char             *buf_,
        size_t                  count_,
        off_t                   offset_)
  {
    return l::write(ffi_,buf_,count_,offset_);
  }

  int
  write_null(const fuse_file_info_t *ffi_,
             const char             *buf_,
             size_t                  count_,
             off_t                   offset_)
  {
    return count_;
  }
}
