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

#include "branches.hpp"
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

#include "fuse.h"
#include "fuse_dirents.h"

#include <string>
#include <vector>

#include <dirent.h>

using std::string;
using std::vector;


namespace l
{
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
    dev_t dev;
    HashSet names;
    string basepath;
    string fullpath;
    uint64_t namelen;

    fuse_dirents_reset(buf_);

    for(const auto &branch : *branches_)
      {
        int rv;
        int dirfd;
        DIR *dh;

        basepath = fs::path::make(branch.path,dirname_);

        dh = fs::opendir(basepath);
        if(!dh)
          continue;

        dirfd = fs::dirfd(dh);
        dev   = fs::devid(dirfd);

        rv = 0;
        for(struct dirent *de = fs::readdir(dh); de && !rv; de = fs::readdir(dh))
          {
            namelen = l::dirent_exact_namelen(de);

            rv = names.put(de->d_name,namelen);
            if(rv == 0)
              continue;

            fullpath = fs::path::make(dirname_,de->d_name);
            de->d_ino = fs::inode::calc(fullpath.c_str(),
                                        fullpath.size(),
                                        DTTOIF(de->d_type),
                                        dev,
                                        de->d_ino);

            rv = fuse_dirents_add(buf_,de,namelen);
            if(rv)
              return (fs::closedir(dh),-ENOMEM);
          }

        fs::closedir(dh);
      }

    return 0;
  }
}

namespace FUSE
{
  int
  readdir_posix(const Branches::CPtr &branches_,
                const char           *dirname_,
                fuse_dirents_t       *buf_)
  {
    return l::readdir(branches_,dirname_,buf_);
  }
}
