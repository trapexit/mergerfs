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

#include "error.hpp"
#include "branches.hpp"
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

#include "fuse.h"
#include "fuse_dirents.h"

#include <string>


static
uint64_t
_dirent_exact_namelen(const struct dirent *d_)
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
_readdir(const Branches::Ptr &branches_,
         const std::string   &rel_dirpath_,
         fuse_dirents_t      *buf_)
{
  Err err;
  HashSet names;
  std::string rel_filepath;
  std::string abs_dirpath;

  fuse_dirents_reset(buf_);

  for(const auto &branch : *branches_)
    {
      int rv;
      DIR *dh;

      abs_dirpath = fs::path::make(branch.path,rel_dirpath_);

      errno = 0;
      dh = fs::opendir(abs_dirpath);
      err = -errno;
      if(!dh)
        continue;

      DEFER{ fs::closedir(dh); };

      rv = 0;
      for(dirent *de = fs::readdir(dh); de; de = fs::readdir(dh))
        {
          std::uint64_t namelen;

          namelen = ::_dirent_exact_namelen(de);

          rv = names.put(de->d_name,namelen);
          if(rv == 0)
            continue;

          rel_filepath = fs::path::make(rel_dirpath_,de->d_name);
          de->d_ino = fs::inode::calc(branch.path,
                                      rel_filepath,
                                      DTTOIF(de->d_type),
                                      de->d_ino);

          rv = fuse_dirents_add(buf_,de,namelen);
          if(rv)
            return -ENOMEM;
        }
    }

  return err;
}

int
FUSE::ReadDirSeq::operator()(fuse_file_info_t const *ffi_,
                             fuse_dirents_t         *buf_)
{
  Config::Read        cfg;
  DirInfo            *di = reinterpret_cast<DirInfo*>(ffi_->fh);
  const fuse_context *fc = fuse_get_context();
  const ugid::Set     ugid(fc->uid,fc->gid);

  return ::_readdir(cfg->branches,
                    di->fusepath,
                    buf_);
}
