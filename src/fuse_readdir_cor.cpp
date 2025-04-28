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

#include "config.hpp"
#include "dirinfo.hpp"
#include "errno.hpp"
#include "fs_close.hpp"
#include "fs_getdents64.hpp"
#include "fs_inode.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "fs_readdir.hpp"
#include "hashset.hpp"
#include "scope_guard.hpp"
#include "ugid.hpp"

#include "fuse_dirents.h"
#include "linux_dirent64.h"


FUSE::ReadDirCOR::ReadDirCOR(unsigned concurrency_,
                             unsigned max_queue_depth_)
  : _tp(concurrency_,max_queue_depth_,"readdir.cor")
{

}

FUSE::ReadDirCOR::~ReadDirCOR()
{

}

namespace l
{
  struct Error
  {
  private:
    int _err;

  public:
    Error()
      : _err(ENOENT)
    {
    }

    operator int()
    {
      return _err;
    }

    Error&
    operator=(int v_)
    {
      if(_err != 0)
        _err = v_;

      return *this;
    }
  };

  static
  inline
  int
  readdir(const std::string &branch_path_,
          const std::string &rel_dirpath_,
          HashSet           &names_,
          fuse_dirents_t    *buf_,
          std::mutex        &mutex_)
  {
    int rv;
    int dfd;
    std::string rel_filepath;
    std::string abs_dirpath;

    abs_dirpath = fs::path::make(branch_path_,rel_dirpath_);

    dfd = fs::open_dir_ro(abs_dirpath);
    if(dfd == -1)
      return errno;

    DEFER{ fs::close(dfd); };

    rv = 0;
    for(;;)
      {
        long nread;
        char buf[32 * 1024];

        nread = fs::getdents_64(dfd,buf,sizeof(buf));
        if(nread == -1)
          return errno;
        if(nread == 0)
          break;

        linux_dirent64_t *d;
        std::lock_guard<std::mutex> lk(mutex_);
        for(long pos = 0; pos < nread; pos += d->reclen)
          {
            std::uint64_t namelen;

            d = (linux_dirent64_t*)&buf[pos];

            namelen = DIRENT_NAMELEN(d);

            rv = names_.put(d->name,namelen);
            if(rv == 0)
              continue;

            rel_filepath = fs::path::make(rel_dirpath_,d->name);
            d->ino = fs::inode::calc(branch_path_,
                                     rel_filepath,
                                     DTTOIF(d->type),
                                     d->ino);

            rv = fuse_dirents_add_linux(buf_,d,namelen);
            if(rv >= 0)
              continue;

            return ENOMEM;
          }
      }

    return 0;
  }

  static
  inline
  int
  concurrent_readdir(ThreadPool           &tp_,
                     const Branches::CPtr &branches_,
                     const std::string    &rel_dirpath_,
                     fuse_dirents_t       *buf_,
                     const uid_t           uid_,
                     const gid_t           gid_)
  {
    HashSet names;
    std::mutex mutex;
    std::vector<std::future<int>> futures;

    fuse_dirents_reset(buf_);
    futures.reserve(branches_->size());

    for(const auto &branch : *branches_)
      {
        auto func =
          [&,buf_,uid_,gid_]()
          {
            const ugid::Set ugid(uid_,gid_);

            return l::readdir(branch.path,
                              rel_dirpath_,
                              names,
                              buf_,
                              mutex);
          };

        auto rv = tp_.enqueue_task(std::move(func));

        futures.emplace_back(std::move(rv));
      }

    Error error;
    for(auto &future : futures)
      error = future.get();

    return -error;
  }
}

int
FUSE::ReadDirCOR::operator()(const fuse_file_info_t *ffi_,
                             fuse_dirents_t         *buf_)
{
  Config::Read        cfg;
  DirInfo            *di = reinterpret_cast<DirInfo*>(ffi_->fh);
  const fuse_context *fc = fuse_get_context();

  return l::concurrent_readdir(_tp,
                               cfg->branches,
                               di->fusepath,
                               buf_,
                               fc->uid,
                               fc->gid);
}
