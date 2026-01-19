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

#include "fuse_release.hpp"

#include "state.hpp"

#include "config.hpp"
#include "fileinfo.hpp"
#include "fs_close.hpp"
#include "fs_fadvise.hpp"
#include "fuse_passthrough.hpp"

#include "fuse.h"


static
int
_release(const fuse_req_ctx_t *ctx_,
         FileInfo             *fi_,
         const bool            dropcacheonclose_)
{
  int       backing_id = INVALID_BACKING_ID;
  FileInfo *map_fi     = nullptr;
  bool      fi_in_map  = false;
  auto     &of         = state.open_files;

  // according to Feh of nocache calling it once doesn't always work
  // https://github.com/Feh/nocache
  if(dropcacheonclose_)
    {
      fs::fadvise_dontneed(fi_->fd);
      fs::fadvise_dontneed(fi_->fd);
    }

  u64 erased;

  erased = of.erase_if(ctx_->nodeid,
                       [&](auto &v_)
                       {
                         fi_in_map = (fi_ == v_.second.fi);

                         v_.second.ref_count--;
                         if(v_.second.ref_count > 0)
                           return false;

                         backing_id = v_.second.backing_id;
                         map_fi     = v_.second.fi;

                         return true;
                       });
  if(not fi_in_map)
    {
      fs::close(fi_->fd);
      delete fi_;
    }

  if(not erased)
    return 0;

  FUSE::passthrough_close(backing_id);
  if(map_fi)
    {
      fs::close(map_fi->fd);
      delete map_fi;
    }

  return 0;
}

int
FUSE::release(const fuse_req_ctx_t   *ctx_,
              const fuse_file_info_t *ffi_)
{
  FileInfo *fi = FileInfo::from_fh(ffi_->fh);

  return ::_release(ctx_,fi,cfg.dropcacheonclose);
}
