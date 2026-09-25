/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_write.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_close.hpp"
#include "fs_dup2.hpp"
#include "fs_movefile_and_open.hpp"
#include "fs_pwrite.hpp"
#include "fs_pwriten.hpp"
#include "ioprio.hpp"
#include "state.hpp"
#include "syslog.hpp"

#include "scope_guard/scope_guard.hpp"

#include "fuse.h"

#include <cstring>
#include <mutex>
#include <shared_mutex>


static
bool
_out_of_space(const ssize_t err_)
{
  return ((err_ == -ENOSPC) ||
          (err_ == -EDQUOT));
}

// moveonenospc: move the file to another branch and rebind fi_->fd to
// the copy. Called with fi_->mutex held exclusively. 0 or -errno.
static
int
_move_file(FileInfo *fi_)
{
  int err;
  int fd;

  if(cfg.moveonenospc.enabled == false)
    return -ENOSPC;

  fd = fs::movefile_and_open_as_root(cfg.moveonenospc.policy,
                                     cfg.branches,
                                     fi_->branch.path,
                                     fi_->fusepath,
                                     fi_->fd);
  if(fd < 0)
    return fd;

  err = fs::dup2(fd,fi_->fd);
  {
    const int close_err = fs::close(fd);
    if(close_err < 0)
      SysLog::warning("moveonenospc: closing the copy's fd after the move"
                      " failed: {}",strerror(-close_err));
  }
  if(err < 0)
    return err;

  fi_->fd_rebound();

  return 0;
}

static
int
_move_and_pwrite(const char   *buf_,
                 const size_t  count_,
                 const off_t   offset_,
                 FileInfo     *fi_,
                 int           err_)
{
  if(::_move_file(fi_) < 0)
    return err_;

  return fs::pwrite(fi_->fd,buf_,count_,offset_);
}

static
int
_move_and_pwriten(char const    *buf_,
                  size_t const   count_,
                  off_t const    offset_,
                  FileInfo      *fi_,
                  ssize_t const  err_,
                  ssize_t const  written_)
{
  int err;
  ssize_t rv;

  if(::_move_file(fi_) < 0)
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
// When not in direct_io mode write's return value is more complex.
// 0 or less than `count` on EOF
// `count` on non-errors
// -errno on error
// See libfuse/include/fuse.h for more details

static
int
_write(const fuse_req_ctx_t   *ctx_,
       const fuse_file_info_t *ffi_,
       const char             *buf_,
       const size_t            count_,
       const off_t             offset_)
{
  FileInfo *fi;

  fi = state.get_fi(ctx_,ffi_->fh);
  if(not fi)
    return -EBADF;

  // Concurrent writes can only happen if:
  // 1) writeback-cache is enabled and using page caching
  // 2) parallel_direct_writes is enabled and file has
  // `direct_io=true`

  ssize_t rv;

  // A writeback (writepage) request must be written completely or
  // fail: the kernel ignores a short count for it (see FUSE::write).
  // So even on a direct_io handle it takes the looping path below,
  // which also reaches moveonenospc when the branch fills mid-write.
  if(fi->direct_io && !ffi_->writepage)
    {
      {
        std::shared_lock<std::shared_mutex> slk(fi->mutex);
        rv = fs::pwrite(fi->fd,buf_,count_,offset_);
      }

      if(not ::_out_of_space(rv))
        return rv;

      std::unique_lock<std::shared_mutex> ulk = fi->lock_exclusive();
      // Re-check under exclusive lock: another writer may have
      // already moved the file and retired. If so the pwrite should
      // now succeed without our own move call.
      rv = fs::pwrite(fi->fd,buf_,count_,offset_);
      if(::_out_of_space(rv))
        rv = ::_move_and_pwrite(buf_,count_,offset_,fi,rv);
      return rv;
    }
  else
    {
      int err;
      ssize_t written;

      {
        std::shared_lock<std::shared_mutex> slk(fi->mutex);
        written = fs::pwriten(fi->fd,buf_,count_,offset_,&err);
      }

      if(err == 0)
        return written;
      if(err && not ::_out_of_space(err))
        return err;

      std::unique_lock<std::shared_mutex> ulk = fi->lock_exclusive();
      // Re-check under exclusive lock as another move may have
      // already run.
      written = fs::pwriten(fi->fd,buf_,count_,offset_,&err);
      if(err == 0)
        return written;
      if(err && not ::_out_of_space(err))
        return err;

      return ::_move_and_pwriten(buf_,count_,offset_,fi,err,written);
    }
}

int
FUSE::write(const fuse_req_ctx_t   *ctx_,
            const fuse_file_info_t *ffi_,
            const char             *buf_,
            size_t                  count_,
            off_t                   offset_)
{
  ioprio::SetFrom iop(ctx_->pid);
  int rv;

  rv = ::_write(ctx_,ffi_,buf_,count_,offset_);

  /* Writeback completion (FUSE_WRITE_CACHE) ignores a short reply's byte
     count: Linux fuse_writepage_end consults only the error, so a short
     nonnegative result would mark the unwritten tail clean and lose it.
     _write already loops for these requests; a short count here means
     a pwrite made no progress without an error, so report a failure.
     Ordinary writes report their prefix. */
  if(ffi_->writepage && (rv >= 0) && ((size_t)rv < count_))
    return -EIO;

  return rv;
}

int
FUSE::write_null(const fuse_req_ctx_t   *ctx_,
                 const fuse_file_info_t *ffi_,
                 const char             *buf_,
                 size_t                  count_,
                 off_t                   offset_)
{
  return count_;
}
