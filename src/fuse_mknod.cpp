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

#include "fuse_mknod.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "error.hpp"
#include "fs_acl.hpp"
#include "fs_mknod_as.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>
#include <vector>

int
FUSE::mknod(const fuse_req_ctx_t *ctx_,
            const char           *fusepath_,
            mode_t                mode_,
            dev_t                 rdev_)
{
  int rv;
  const fs::path fusepath{fusepath_};
  const ugid_t ugid(ctx_);

  rv = cfg.mknod(ugid,
                 cfg.branches,
                 fusepath,
                 mode_,
                 ctx_->umask,
                 rdev_);
  if(rv == -EROFS)
    {
      cfg.branches.find_and_set_mode_ro();
      rv = cfg.mknod(ugid,
                     cfg.branches,
                     fusepath,
                     mode_,
                     ctx_->umask,
                     rdev_);
    }

  return rv;
}
