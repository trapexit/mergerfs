/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_acl.hpp"
#include "fs_base_open.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

#include <fuse.h>

#include <string>
#include <vector>

using std::string;
using std::vector;
typedef Config::CacheFiles CacheFiles;

namespace l
{
  static
  int
  create_core(const string &fullpath_,
              mode_t        mode_,
              const mode_t  umask_,
              const int     flags_)
  {
    if(!fs::acl::dir_has_defaults(fullpath_))
      mode_ &= ~umask_;

    return fs::open(fullpath_,flags_,mode_);
  }

  static
  int
  create_core(const string &createpath_,
              const char   *fusepath_,
              const mode_t  mode_,
              const mode_t  umask_,
              const int     flags_,
              uint64_t     *fh_)
  {
    int rv;
    string fullpath;

    fullpath = fs::path::make(createpath_,fusepath_);

    rv = l::create_core(fullpath,mode_,umask_,flags_);
    if(rv == -1)
      return -errno;

    *fh_ = reinterpret_cast<uint64_t>(new FileInfo(rv,fusepath_));

    return 0;
  }

  static
  int
  create(Policy::Func::Search  searchFunc_,
         Policy::Func::Create  createFunc_,
         const Branches       &branches_,
         const uint64_t        minfreespace_,
         const char           *fusepath_,
         const mode_t          mode_,
         const mode_t          umask_,
         const int             flags_,
         uint64_t             *fh_)
  {
    int rv;
    string fullpath;
    string fusedirpath;
    vector<const string*> createpaths;
    vector<const string*> existingpaths;

    fusedirpath = fs::path::dirname(fusepath_);

    rv = searchFunc_(branches_,fusedirpath,minfreespace_,existingpaths);
    if(rv == -1)
      return -errno;

    rv = createFunc_(branches_,fusedirpath,minfreespace_,createpaths);
    if(rv == -1)
      return -errno;

    rv = fs::clonepath_as_root(*existingpaths[0],*createpaths[0],fusedirpath);
    if(rv == -1)
      return -errno;

    return l::create_core(*createpaths[0],
                          fusepath_,
                          mode_,
                          umask_,
                          flags_,
                          fh_);
  }
}

namespace FUSE
{
  int
  create(const char     *fusepath_,
         mode_t          mode_,
         fuse_file_info *ffi_)
  {
    const fuse_context      *fc     = fuse_get_context();
    const Config            &config = Config::get(fc);
    const ugid::Set          ugid(fc->uid,fc->gid);
    const rwlock::ReadGuard  readlock(&config.branches_lock);

    switch(config.cache_files)
      {
      case CacheFiles::LIBFUSE:
        ffi_->direct_io  = config.direct_io;
        ffi_->keep_cache = config.kernel_cache;
        ffi_->auto_cache = config.auto_cache;
        break;
      case CacheFiles::OFF:
        ffi_->direct_io  = 1;
        ffi_->keep_cache = 0;
        ffi_->auto_cache = 0;
        break;
      case CacheFiles::PARTIAL:
        ffi_->direct_io  = 0;
        ffi_->keep_cache = 0;
        ffi_->auto_cache = 0;
        break;
      case CacheFiles::FULL:
        ffi_->direct_io  = 0;
        ffi_->keep_cache = 1;
        ffi_->auto_cache = 0;
        break;
      case CacheFiles::AUTO_FULL:
        ffi_->direct_io  = 0;
        ffi_->keep_cache = 0;
        ffi_->auto_cache = 1;
        break;
      }

    return l::create(config.getattr,
                     config.create,
                     config.branches,
                     config.minfreespace,
                     fusepath_,
                     mode_,
                     fc->umask,
                     ffi_->flags,
                     &ffi_->fh);
  }
}
