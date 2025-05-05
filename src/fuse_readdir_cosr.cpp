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
#include "errno.hpp"
#include "fs_closedir.hpp"
#include "fs_dirfd.hpp"
#include "fs_inode.hpp"
#include "fs_opendir.hpp"
#include "fs_path.hpp"
#include "fs_readdir.hpp"
#include "fs_stat.hpp"
#include "hashset.hpp"
#include "scope_guard.hpp"
#include "ugid.hpp"

#include "fuse_dirents.h"


FUSE::ReadDirCOSR::ReadDirCOSR(unsigned concurrency_,
                               unsigned max_queue_depth_)
  : _tp(concurrency_,max_queue_depth_,"readdir.cosr")
{

}

FUSE::ReadDirCOSR::~ReadDirCOSR()
{

}

namespace l
{
  struct DirRV
  {
    const std::string *branch_path;
    DIR *dir;
    int  err;
  };

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
    operator=(int const v_)
    {
      if(_err != 0)
        _err = v_;

      return *this;
    }
  };

  static
  uint64_t
  dirent_exact_namelen(const struct dirent *d_)
  {
#ifdef _D_EXACT_NAMLEN
    return _D_EXACT_NAMLEN(d_);
#elif defined _DIRENT_HAVE_D_NAMLEN
    return d_->d_namlen;
#else
    return strlen(d_->d_name);
#endif
  }

  static
  inline
  std::vector<std::future<DirRV>>
  opendir(ThreadPool           &tp_,
          const Branches::CPtr &branches_,
          const std::string    &rel_dirpath_,
          uid_t const           uid_,
          gid_t const           gid_)
  {
    std::vector<std::future<DirRV>> futures;

    futures.reserve(branches_->size());

    for(const auto &branch : *branches_)
      {
        auto func =
          [&branch,&rel_dirpath_,uid_,gid_]()
          {
            DIR *dir;
            std::string abs_dirpath;
            const ugid::Set ugid(uid_,gid_);

            abs_dirpath = fs::path::make(branch.path,rel_dirpath_);

            errno  = 0;
            dir = fs::opendir(abs_dirpath);

            return DirRV{&branch.path,dir,errno};
          };

        auto rv = tp_.enqueue_task(std::move(func));

        futures.emplace_back(std::move(rv));
      }

    return futures;
  }

  static
  inline
  int
  readdir(std::vector<std::future<DirRV>> &dh_futures_,
          const std::string               &rel_dirpath_,
          fuse_dirents_t                  *buf_)
  {
    Error error;
    HashSet names;
    std::string rel_filepath;

    for(auto &dh_future : dh_futures_)
      {
        int rv;
        DirRV dirrv;

        dirrv = dh_future.get();

        error = dirrv.err;
        if(dirrv.dir == NULL)
          continue;

        DEFER { fs::closedir(dirrv.dir); };

        rv = 0;
        for(dirent *de = fs::readdir(dirrv.dir); de && !rv; de = fs::readdir(dirrv.dir))
          {
            std::uint64_t namelen;

            namelen = l::dirent_exact_namelen(de);

            rv = names.put(de->d_name,namelen);
            if(rv == 0)
              continue;

            rel_filepath = fs::path::make(rel_dirpath_,de->d_name);
            de->d_ino    = fs::inode::calc(*dirrv.branch_path,
                                           rel_filepath,
                                           DTTOIF(de->d_type),
                                           de->d_ino);

            rv = fuse_dirents_add(buf_,de,namelen);
            if(rv == 0)
              continue;

            error = ENOMEM;
          }
      }

    return -error;
  }

  static
  inline
  int
  readdir(ThreadPool           &tp_,
          const Branches::CPtr &branches_,
          const std::string    &rel_dirpath_,
          fuse_dirents_t       *buf_,
          uid_t const           uid_,
          gid_t const           gid_)
  {
    int rv;
    std::vector<std::future<DirRV>> futures;

    fuse_dirents_reset(buf_);

    futures = l::opendir(tp_,branches_,rel_dirpath_,uid_,gid_);
    rv      = l::readdir(futures,rel_dirpath_,buf_);

    return rv;
  }
}

int
FUSE::ReadDirCOSR::operator()(fuse_file_info_t const *ffi_,
                              fuse_dirents_t         *buf_)
{
  Config::Read        cfg;
  DirInfo            *di = reinterpret_cast<DirInfo*>(ffi_->fh);
  const fuse_context *fc = fuse_get_context();

  return l::readdir(_tp,
                    cfg->branches,
                    di->fusepath,
                    buf_,
                    fc->uid,
                    fc->gid);
}
