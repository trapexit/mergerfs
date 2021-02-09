/*
  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "branches.hpp"
#include "errno.hpp"
#include "fs_close.hpp"
#include "fs_devid.hpp"
#include "fs_getdents64.hpp"
#include "fs_inode.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "fs_stat.hpp"
#include "hashset.hpp"
#include "linux_dirent64.h"
#include "mempools.hpp"

#include "fuse.h"
#include "fuse_dirents.h"

#include <string>
#include <vector>

#include <stddef.h>

using std::string;
using std::vector;

namespace l
{
  static
  int
  close_free_ret_enomem(int   fd_,
                        void *buf_)
  {
    fs::close(fd_);
    g_DENTS_BUF_POOL.free(buf_);
    return -ENOMEM;
  }

  static
  int
  readdir(const Branches::CPtr &branches_,
          const char           *dirname_,
          fuse_dirents_t       *buf_)
  {
    int rv;
    dev_t dev;
    char *buf;
    HashSet names;
    string basepath;
    string fullpath;
    uint64_t namelen;
    struct linux_dirent64 *d;

    fuse_dirents_reset(buf_);

    buf = (char*)g_DENTS_BUF_POOL.alloc();
    if(buf == NULL)
      return -ENOMEM;

    for(const auto &branch : *branches_)
      {
        int dirfd;
        int64_t nread;

        basepath = fs::path::make(branch.path,dirname_);

        dirfd = fs::open_dir_ro(basepath);
        if(dirfd == -1)
          continue;

        dev = fs::devid(dirfd);

        for(;;)
          {
            nread = fs::getdents_64(dirfd,buf,g_DENTS_BUF_POOL.size());
            if(nread == -1)
              break;
            if(nread == 0)
              break;

            for(int64_t pos = 0; pos < nread; pos += d->reclen)
              {
                d = (struct linux_dirent64*)(buf + pos);
                namelen = strlen(d->name);

                rv = names.put(d->name,namelen);
                if(rv == 0)
                  continue;

                fullpath = fs::path::make(dirname_,d->name);
                d->ino = fs::inode::calc(fullpath.c_str(),
                                         fullpath.size(),
                                         DTTOIF(d->type),
                                         dev,
                                         d->ino);

                rv = fuse_dirents_add_linux(buf_,d,namelen);
                if(rv)
                  return close_free_ret_enomem(dirfd,buf);
              }
          }

        fs::close(dirfd);
      }

    g_DENTS_BUF_POOL.free(buf);

    return 0;
  }
}

namespace FUSE
{
  int
  readdir_linux(const Branches::CPtr &branches_,
                const char           *dirname_,
                fuse_dirents_t       *buf_)
  {
    return l::readdir(branches_,dirname_,buf_);
  }
}
