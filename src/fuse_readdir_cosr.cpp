/*
  Copyright (c) 2023, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_readdir_cosr.hpp"

#include "config.hpp"
#include "dirinfo.hpp"
#include "supported_getdents64.hpp"


FUSE::ReadDirCOSR::ReadDirCOSR(unsigned concurrency_,
                               unsigned max_queue_depth_)
  : _tp(concurrency_,max_queue_depth_,"readdir.cosr")
{

}

FUSE::ReadDirCOSR::~ReadDirCOSR()
{

}

#ifdef MERGERFS_SUPPORTED_GETDENTS64
#include "fuse_readdir_cosr_getdents.icpp"
#else
#include "fuse_readdir_cosr_readdir.icpp"
#endif

static
inline
int
_readdir(ThreadPool          &tp_,
         const Branches::Ptr &branches_,
         const fs::path      &rel_dirpath_,
         fuse_dirents_t      *dirents_)
{
  int rv;
  std::vector<std::future<DirRV>> futures;

  fuse_dirents_reset(dirents_);

  futures = ::_opendir(tp_,branches_,rel_dirpath_);
  rv      = ::_readdir(futures,rel_dirpath_,dirents_);

  return rv;
}

int
FUSE::ReadDirCOSR::operator()(const fuse_req_ctx_t   *ctx_,
                              fuse_file_info_t const *ffi_,
                              fuse_dirents_t         *dirents_)
{
  DirInfo *di = DirInfo::from_fh(ffi_->fh);

  return ::_readdir(_tp,
                    cfg.branches,
                    di->fusepath,
                    dirents_);
}
