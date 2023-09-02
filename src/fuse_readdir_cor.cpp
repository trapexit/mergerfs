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
#include "fs_closedir.hpp"
#include "fs_devid.hpp"
#include "fs_inode.hpp"
#include "fs_opendir.hpp"
#include "fs_path.hpp"
#include "fs_readdir.hpp"
#include "hashset.hpp"
#include "ugid.hpp"

#include "fuse_dirents.h"


FUSE::ReadDirCOR::ReadDirCOR(unsigned concurrency_)
  : _tp(concurrency_,"readdir.cor")
{

}

FUSE::ReadDirCOR::~ReadDirCOR()
{

}

namespace l
{
  static
  inline
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
  int
  readdir(std::string     basepath_,
          HashSet        &names_,
          std::mutex     &names_mutex_,
          fuse_dirents_t *buf_,
          std::mutex     &dirents_mutex_)
  {
    int rv;
    int err;
    DIR *dh;
    dev_t dev;
    std::string filepath;

    dh = fs::opendir(basepath_);
    if(dh == NULL)
      return -errno;

    dev = fs::devid(dh);

    rv = 0;
    err = 0;
    for(struct dirent *de = fs::readdir(dh); de && !rv; de = fs::readdir(dh))
      {
        std::uint64_t namelen;

        namelen = l::dirent_exact_namelen(de);

        {
          std::lock_guard<std::mutex> lk(names_mutex_);
          rv = names_.put(de->d_name,namelen);
          if(rv == 0)
            continue;
        }

        filepath  = fs::path::make(basepath_,de->d_name);
        de->d_ino = fs::inode::calc(filepath,
                                    DTTOIF(de->d_type),
                                    dev,
                                    de->d_ino);

        {
          std::lock_guard<std::mutex> lk(dirents_mutex_);
          rv = fuse_dirents_add(buf_,de,namelen);
          if(rv == 0)
            continue;
        }

        err = -ENOMEM;
      }

    fs::closedir(dh);

    return err;
  }

  static
  std::vector<int>
  concurrent_readdir(UnboundedThreadPool  &tp_,
                     const Branches::CPtr &branches_,
                     const char           *dirname_,
                     fuse_dirents_t       *buf_,
                     uid_t const           uid_,
                     gid_t const           gid_)
  {
    HashSet names;
    std::mutex names_mutex;
    std::mutex dirents_mutex;
    std::vector<int> rv;
    std::vector<std::future<int>> futures;

    for(auto const &branch : *branches_)
      {
        auto func = [&,dirname_,buf_,uid_,gid_]()
        {
          std::string basepath;
          ugid::Set const ugid(uid_,gid_);

          basepath = fs::path::make(branch.path,dirname_);

          return l::readdir(basepath,names,names_mutex,buf_,dirents_mutex);
        };

        auto rv = tp_.enqueue_task(func);

        futures.emplace_back(std::move(rv));
      }

    for(auto &future : futures)
      rv.push_back(future.get());

    return rv;
  }

  static
  int
  calc_rv(std::vector<int> rvs_)
  {
    for(auto rv : rvs_)
      {
        if(rv == 0)
          return 0;
      }

    if(rvs_.empty())
      return -ENOENT;

    return rvs_[0];
  }

  static
  int
  readdir(UnboundedThreadPool  &tp_,
          const Branches::CPtr &branches_,
          const char           *dirname_,
          fuse_dirents_t       *buf_,
          uid_t const           uid_,
          gid_t const           gid_)
  {
    std::vector<int> rvs;

    fuse_dirents_reset(buf_);

    rvs = l::concurrent_readdir(tp_,branches_,dirname_,buf_,uid_,gid_);

    return l::calc_rv(rvs);
  }
}

int
FUSE::ReadDirCOR::operator()(fuse_file_info_t const *ffi_,
                             fuse_dirents_t         *buf_)
{
  Config::Read        cfg;
  DirInfo            *di = reinterpret_cast<DirInfo*>(ffi_->fh);
  const fuse_context *fc = fuse_get_context();

  return l::readdir(_tp,
                    cfg->branches,
                    di->fusepath.c_str(),
                    buf_,
                    fc->uid,
                    fc->gid);
}
