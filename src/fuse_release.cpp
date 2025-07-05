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

#include "state.hpp"

#include "config.hpp"
#include "fileinfo.hpp"
#include "fs_close.hpp"
#include "fs_fadvise.hpp"
#include "fuse_passthrough.hpp"

#include "fuse.h"

static
constexpr
auto
_erase_if_lambda(const fuse_context *fc_,
                 FileInfo           *fi_,
                 bool               *existed_in_map_)
{
  return
    [=](auto &val_)
    {
      *existed_in_map_ = true;

      if(fi_ != val_.second.fi)
        {
          fs::close(fi_->fd);
          delete fi_;
        }

      val_.second.ref_count--;
      if(val_.second.ref_count > 0)
        return false;

      fmt::print("release backing id: {}\n",val_.second.backing_id);
      if(val_.second.backing_id > 0)
        FUSE::passthrough_close(fc_,val_.second.backing_id);
      fs::close(val_.second.fi->fd);
      delete val_.second.fi;

      return true;
    };
}

static
int
_release(const fuse_context *fc_,
         FileInfo           *fi_,
         const bool          dropcacheonclose_)
{
  bool existed_in_map;

  // according to Feh of nocache calling it once doesn't always work
  // https://github.com/Feh/nocache
  if(dropcacheonclose_)
    {
      fs::fadvise_dontneed(fi_->fd);
      fs::fadvise_dontneed(fi_->fd);
    }

  // Because of course the API doesn't tell you if the key
  // existed. Just how many it erased and in this case I only want to
  // erase if there are no more open files.
  existed_in_map = false;
  state.open_files.erase_if(fc_->nodeid,
                            ::_erase_if_lambda(fc_,fi_,&existed_in_map));
  if(existed_in_map)
    return 0;

  fs::close(fi_->fd);
  delete fi_;

  return 0;
}

static
int
_release(const fuse_context     *fc_,
         const fuse_file_info_t *ffi_)
{
  Config::Read cfg;
  FileInfo *fi = reinterpret_cast<FileInfo*>(ffi_->fh);

  return ::_release(fc_,fi,cfg->dropcacheonclose);
}

namespace FUSE
{
  int
  release(const fuse_file_info_t *ffi_)
  {
    const fuse_context *fc = fuse_get_context();

    return ::_release(fc,ffi_);
  }
}
