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
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_close.hpp"
#include "fs_fadvise.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <string>

static
constexpr
auto
_erase_if_lambda(FileInfo *fi_,
                 bool     *existed_in_map_)
{
  return
    [=](auto &val)
    {
      *existed_in_map_ = true;

      val.second.ref_count--;
      if(val.second.ref_count > 0)
        return false;

      const ugid::SetRootGuard ugid;
      const fuse_context *fc = fuse_get_context();
      if(val.second.backing_id > 0)
        fuse_passthrough_close(fc,val.second.backing_id);
      fs::close(fi_->fd);
      delete fi_;

      return true;
    };
}

static
int
_release(FileInfo   *fi_,
         const bool  dropcacheonclose_)
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
  state.passthrough.erase_if(fi_->fusepath,
                             ::_erase_if_lambda(fi_,&existed_in_map));
  if(existed_in_map)
    return 0;

  fs::close(fi_->fd);
  delete fi_;

  return 0;
}

static
int
_release(const fuse_file_info_t *ffi_)
{
  Config::Read cfg;
  FileInfo *fi = reinterpret_cast<FileInfo*>(ffi_->fh);

  return ::_release(fi,cfg->dropcacheonclose);
}

namespace FUSE
{
  int
  release(const fuse_file_info_t *ffi_)
  {
    return ::_release(ffi_);
  }
}
