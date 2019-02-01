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

#include "config.hpp"
#include "dirinfo.hpp"
#include "errno.hpp"
#include "fs_base_closedir.hpp"
#include "fs_base_dirfd.hpp"
#include "fs_base_opendir.hpp"
#include "fs_base_readdir.hpp"
#include "fs_base_stat.hpp"
#include "fs_devid.hpp"
#include "fs_inode.hpp"
#include "fs_path.hpp"
#include "hashset.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

#include <fuse.h>

#include <string>
#include <vector>

using std::string;
using std::vector;

#define NO_OFFSET 0

namespace l
{
  static
  int
  readdir(const Branches        &branches_,
          const char            *dirname_,
          void                  *buf_,
          const fuse_fill_dir_t  filler_)
  {
    HashSet names;
    string basepath;
    struct stat st = {0};

    for(size_t i = 0, ei = branches_.size(); i != ei; i++)
      {
        int rv;
        int dirfd;
        DIR *dh;

        basepath = fs::path::make(&branches_[i].path,dirname_);

        dh = fs::opendir(basepath);
        if(!dh)
          continue;

        dirfd     = fs::dirfd(dh);
        st.st_dev = fs::devid(dirfd);
        if(st.st_dev == (dev_t)-1)
          st.st_dev = i;

        rv = 0;
        for(struct dirent *de = fs::readdir(dh); de && !rv; de = fs::readdir(dh))
          {
            rv = names.put(de->d_name);
            if(rv == 0)
              continue;

            st.st_ino  = de->d_ino;
            st.st_mode = DTTOIF(de->d_type);

            fs::inode::recompute(&st);

            rv = filler_(buf_,de->d_name,&st,NO_OFFSET);
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
  readdir(const char      *fusepath_,
          void            *buf_,
          fuse_fill_dir_t  filler_,
          off_t            offset_,
          fuse_file_info  *ffi_)
  {
    DirInfo                 *di     = reinterpret_cast<DirInfo*>(ffi_->fh);
    const fuse_context      *fc     = fuse_get_context();
    const Config            &config = Config::get(fc);
    const ugid::Set          ugid(fc->uid,fc->gid);
    const rwlock::ReadGuard  readlock(&config.branches_lock);

    return l::readdir(config.branches,
                      di->fusepath.c_str(),
                      buf_,
                      filler_);
  }
}
