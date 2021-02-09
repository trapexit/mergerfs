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
#include "fs_movefile.hpp"
#include "fs_write.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>
#include <vector>

using std::string;
using std::vector;

typedef int (*WriteFunc)(const int,const void*,const size_t,const off_t);


namespace l
{
  static
  bool
  out_of_space(const int error_)
  {
    return ((error_ == ENOSPC) ||
            (error_ == EDQUOT));
  }

  static
  int
  write_regular(const int     fd_,
                const void   *buf_,
                const size_t  count_,
                const off_t   offset_)
  {
    int rv;

    rv = fs::pwrite(fd_,buf_,count_,offset_);
    if(rv == -1)
      return -errno;
    if(rv == 0)
      return 0;

    return count_;
  }

  static
  int
  write_direct_io(const int     fd_,
                  const void   *buf_,
                  const size_t  count_,
                  const off_t   offset_)
  {
    int rv;

    rv = fs::pwrite(fd_,buf_,count_,offset_);
    if(rv == -1)
      return -errno;

    return rv;
  }

  static
  int
  move_and_write(WriteFunc     func_,
                 const char   *buf_,
                 const size_t  count_,
                 const off_t   offset_,
                 FileInfo     *fi_,
                 int           err_)
  {
    int rv;
    Config::Read cfg;

    if(cfg->moveonenospc.enabled == false)
      return err_;

    rv = fs::movefile_as_root(cfg->moveonenospc.policy,
                              cfg->branches,
                              fi_->fusepath,
                              &fi_->fd);
    if(rv == -1)
      return err_;

    return func_(fi_->fd,buf_,count_,offset_);
  }

  static
  int
  write(const fuse_file_info_t *ffi_,
        WriteFunc               func_,
        const char             *buf_,
        const size_t            count_,
        const off_t             offset_)
  {
    int rv;
    FileInfo* fi;

    fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    rv = func_(fi->fd,buf_,count_,offset_);
    if(l::out_of_space(-rv))
      rv = l::move_and_write(func_,buf_,count_,offset_,fi,rv);

    return rv;
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
    WriteFunc wf;

    wf = ((ffi_->direct_io) ?
          l::write_direct_io :
          l::write_regular);

    return l::write(ffi_,wf,buf_,count_,offset_);
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
