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

#include "fuse_fgetattr.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_fstat.hpp"
#include "fs_inode.hpp"
#include "state.hpp"

#include "fuse.h"


static
int
_fgetattr(const FileInfo  *fi_,
          struct stat     *st_)
{
  int rv;

  rv = fs::fstat(fi_->fd,st_);
  if(rv < 0)
    return rv;

  fs::inode::calc(fi_->branch.path,
                  fi_->fusepath,
                  st_);

  return rv;
}


int
FUSE::fgetattr(const uint64_t   fh_,
               struct stat     *st_,
               fuse_timeouts_t *timeout_)
{
  int rv;
  uint64_t fh;
  Config::Read cfg;
  const fuse_context *fc = fuse_get_context();

  fh = fh_;
  if(fh == 0)
    {
      state.open_files.cvisit(fc->nodeid,
                              [&](const auto &val_)
                              {
                                fh = reinterpret_cast<uint64_t>(val_.second.fi);
                              });
    }

  if(fh == 0)
    {
      timeout_->entry = cfg->cache_negative_entry;
      return -ENOENT;
    }

  FileInfo *fi = reinterpret_cast<FileInfo*>(fh);

  rv = ::_fgetattr(fi,st_);

  timeout_->entry = ((rv >= 0) ?
                     cfg->cache_entry :
                     cfg->cache_negative_entry);
  timeout_->attr  = cfg->cache_attr;

  return rv;
}
