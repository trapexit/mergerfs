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

#include "fuse_readdir_cor.hpp"

#include "supported_getdents64.hpp"

#include "config.hpp"
#include "dirinfo.hpp"
#include "error.hpp"
#include "ugid.hpp"


FUSE::ReadDirCOR::ReadDirCOR(unsigned concurrency_,
                             unsigned max_queue_depth_)
  : _tp(concurrency_,max_queue_depth_,"readdir.cor")
{

}

FUSE::ReadDirCOR::~ReadDirCOR()
{

}

#ifdef MERGERFS_SUPPORTED_GETDENTS64
#include "fuse_readdir_cor_getdents.icpp"
#else
#include "fuse_readdir_cor_readdir.icpp"
#endif

static
inline
int
_concurrent_readdir(ThreadPool          &tp_,
                    const Branches::Ptr &branches_,
                    const fs::path      &rel_dirpath_,
                    fuse_dirents_t      *dirents_,
                    const uid_t          uid_,
                    const gid_t          gid_)
{
  HashSet names;
  std::mutex mutex;
  std::vector<std::future<int>> futures;

  fuse_dirents_reset(dirents_);
  futures.reserve(branches_->size());

  for(const auto &branch : *branches_)
    {
      auto func =
        [&,dirents_,uid_,gid_]()
        {
          const ugid::Set ugid(uid_,gid_);

          return ::_readdir(branch.path,
                            rel_dirpath_,
                            names,
                            dirents_,
                            mutex);
        };

      auto rv = tp_.enqueue_task(std::move(func));

      futures.emplace_back(std::move(rv));
    }

  Err err;
  for(auto &future : futures)
    err = future.get();

  return err;
}

int
FUSE::ReadDirCOR::operator()(const fuse_req_ctx_t   *ctx_,
                             const fuse_file_info_t *ffi_,
                             fuse_dirents_t         *dirents_)
{
  DirInfo *di = DirInfo::from_fh(ffi_->fh);

  return ::_concurrent_readdir(_tp,
                               cfg.branches,
                               di->fusepath,
                               dirents_,
                               ctx_->uid,
                               ctx_->gid);
}
