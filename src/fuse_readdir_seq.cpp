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

#include "branches.hpp"
#include "config.hpp"
#include "dirinfo.hpp"
#include "errno.hpp"
#include "fs_closedir.hpp"
#include "fs_devid.hpp"
#include "fs_dirfd.hpp"
#include "fs_inode.hpp"
#include "fs_opendir.hpp"
#include "fs_path.hpp"
#include "fs_readdir.hpp"
#include "fs_stat.hpp"
#include "hashset.hpp"
#include "scope_guard.hpp"
#include "ugid.hpp"

#include "fuse.h"
#include "fuse_dirents.h"

#include <string>


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
  int
  readdir(const Branches::CPtr &branches_,
          const char           *dirname_,
          fuse_dirents_t       *buf_)
  {
    Error error;
    HashSet names;
    std::string basepath;
    std::string fullpath;

    fuse_dirents_reset(buf_);

    for(auto const &branch : *branches_)
      {
        int rv;
        DIR *dh;
        dev_t dev;

        basepath = fs::path::make(branch.path,dirname_);

        errno = 0;
        dh = fs::opendir(basepath);
        error = errno;
        if(!dh)
          continue;

        DEFER{ fs::closedir(dh); };

        dev = fs::devid(dh);

        rv = 0;
        for(dirent *de = fs::readdir(dh); de && !rv; de = fs::readdir(dh))
          {
            std::uint64_t namelen;

            namelen = l::dirent_exact_namelen(de);

            rv = names.put(de->d_name,namelen);
            if(rv == 0)
              continue;

            fullpath = fs::path::make(dirname_,de->d_name);
            de->d_ino = fs::inode::calc(fullpath,
                                        DTTOIF(de->d_type),
                                        dev,
                                        de->d_ino);

            rv = fuse_dirents_add(buf_,de,namelen);
            if(rv)
              return -ENOMEM;
          }
      }

    return -error;
  }
}

int
FUSE::ReadDirSeq::operator()(fuse_file_info_t const *ffi_,
                             fuse_dirents_t         *buf_)
{
  Config::Read        cfg;
  DirInfo            *di = reinterpret_cast<DirInfo*>(ffi_->fh);
  const fuse_context *fc = fuse_get_context();
  const ugid::Set     ugid(fc->uid,fc->gid);

  return l::readdir(cfg->branches,
                    di->fusepath.c_str(),
                    buf_);
}
