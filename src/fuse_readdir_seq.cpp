/*
  Copyright (c) 2019, Antonio SJ Musumeci <trapexit@spawn.link>

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

#define _DEFAULT_SOURCE

#include "fuse_readdir_seq.hpp"

#include "config.hpp"
#include "supported_getdents64.hpp"
#include "ugid.hpp"

#ifdef MERGERFS_SUPPORTED_GETDENTS64
#include "fuse_readdir_seq_getdents.icpp"
#else
#include "fuse_readdir_seq_readdir.icpp"
#endif

int
FUSE::ReadDirSeq::opendir(const fuse_req_ctx_t *ctx_,
                          const char           *fusepath_,
                          fuse_file_info_t     *ffi_)
{
  DirInfo *di;

  di = new DirInfo(fusepath_);

  ffi_->fh = di->to_fh();

  ffi_->noflush = true;

  if(cfg.cache_readdir)
    {
      ffi_->keep_cache    = true;
      ffi_->cache_readdir = true;
    }

  Branches::Impl branches = cfg.branches;

  return 0;
}

int
FUSE::ReadDirSeq::readdir(const fuse_req_ctx_t   *ctx_,
                          const fuse_file_info_t *ffi_,
                          fuse_dirents_t         *dirents_)
{
  DirInfo *di = DirInfo::from_fh(ffi_->fh);

  return ::_readdir(cfg.branches,
                    di->fusepath,
                    dirents_);
}
