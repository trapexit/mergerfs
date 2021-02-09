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

#include "branches.hpp"
#include "errno.hpp"
#include "fs_close.hpp"
#include "fs_devid.hpp"
#include "fs_fstatat.hpp"
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
  readdir_plus(const Branches::CPtr &branches_,
               const char           *dirname_,
               const uint64_t        entry_timeout_,
               const uint64_t        attr_timeout_,
               fuse_dirents_t       *buf_)
  {
    int rv;
    dev_t dev;
    char *buf;
    HashSet names;
    string basepath;
    string fullpath;
    uint64_t namelen;
    struct stat st;
    fuse_entry_t entry;
    struct linux_dirent64 *d;

    fuse_dirents_reset(buf_);

    buf = (char*)g_DENTS_BUF_POOL.alloc();

    entry.nodeid           = 0;
    entry.generation       = 0;
    entry.entry_valid      = entry_timeout_;
    entry.attr_valid       = attr_timeout_;
    entry.entry_valid_nsec = 0;
    entry.attr_valid_nsec  = 0;
    for(auto &branch : *branches_)
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
                namelen = (strlen(d->name) + 1);

                rv = names.put(d->name,namelen);
                if(rv == 0)
                  continue;

                rv = fs::fstatat_nofollow(dirfd,d->name,&st);
                if(rv == -1)
                  {
                    memset(&st,0,sizeof(st));
                    st.st_ino  = d->ino;
                    st.st_dev  = dev;
                    st.st_mode = DTTOIF(d->type);
                  }

                fullpath = fs::path::make(dirname_,d->name);
                fs::inode::calc(fullpath,&st);
                d->ino = st.st_ino;

                rv = fuse_dirents_add_linux_plus(buf_,d,namelen,&entry,&st);
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
  readdir_plus_linux(const Branches::CPtr &branches_,
                     const char           *dirname_,
                     const uint64_t        entry_timeout_,
                     const uint64_t        attr_timeout_,
                     fuse_dirents_t       *buf_)
  {
    return l::readdir_plus(branches_,dirname_,entry_timeout_,attr_timeout_,buf_);
  }
}
