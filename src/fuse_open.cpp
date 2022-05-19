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

#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_lchmod.hpp"
#include "fs_cow.hpp"
#include "fs_fchmod.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "fs_stat.hpp"
#include "stat_util.hpp"
#include "ugid.hpp"
#include "state.hpp"

#include "fuse.h"

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace l
{
  /*
    The kernel expects being able to issue read requests when running
    with writeback caching enabled so we must change O_WRONLY to
    O_RDWR.

    With writeback caching enabled the kernel handles O_APPEND. Could
    be an issue if the underlying file changes out of band but that is
    true of any caching.
  */
  static
  void
  tweak_flags_writeback_cache(int *flags_)
  {
    if((*flags_ & O_ACCMODE) == O_WRONLY)
      *flags_ = ((*flags_ & ~O_ACCMODE) | O_RDWR);
    if(*flags_ & O_APPEND)
      *flags_ &= ~O_APPEND;
  }
}

namespace FUSE::OPEN
{
  int
  open(const char       *fusepath_,
       fuse_file_info_t *ffi_)
  {
    State s;
    const fuse_context *fc  = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    if(s->writeback_cache)
      l::tweak_flags_writeback_cache(&ffi_->flags);

    return s->open(fusepath_,ffi_);
  }
}
