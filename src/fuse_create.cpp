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
#include "fs_clonepath.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "ugid.hpp"
#include "func_open_create_utils.hpp"

#include "fuse.h"

#include <string>
#include <vector>


namespace l
{
  static
  int
  create_core(const std::string &fullpath_,
              mode_t             mode_,
              const mode_t       umask_,
              const int          flags_)
  {
    if(!fs::acl::dir_has_defaults(fullpath_))
      mode_ &= ~umask_;

    return fs::open(fullpath_,flags_,mode_);
  }

  static
  int
  create_core(const std::string &createpath_,
              const char        *fusepath_,
              fuse_file_info_t  *ffi_,
              const mode_t       mode_,
              const mode_t       umask_)
  {
    int rv;
    FileInfo *fi;
    std::string fullpath;

    fullpath = fs::path::make(createpath_,fusepath_);

    rv = l::create_core(fullpath,mode_,umask_,ffi_->flags);
    if(rv == -1)
      return -errno;

    fi = new FileInfo(rv,fusepath_,ffi_->direct_io);

    ffi_->fh = reinterpret_cast<uint64_t>(fi);

    return 0;
  }

  static
  int
  create(const Policy::Search &searchFunc_,
         const Policy::Create &createFunc_,
         const Branches       &branches_,
         const char           *fusepath_,
         fuse_file_info_t     *ffi_,
         const mode_t          mode_,
         const mode_t          umask_)
  {
    int rv;
    std::string fullpath;
    std::string fusedirpath;
    StrVec createpaths;
    StrVec existingpaths;

    fusedirpath = fs::path::dirname(fusepath_);

    rv = searchFunc_(branches_,fusedirpath,&existingpaths);
    if(rv == -1)
      return -errno;

    rv = createFunc_(branches_,fusedirpath,&createpaths);
    if(rv == -1)
      return -errno;

    rv = fs::clonepath_as_root(existingpaths[0],createpaths[0],fusedirpath);
    if(rv == -1)
      return -errno;

    return l::create_core(createpaths[0],
                          fusepath_,
                          ffi_,
                          mode_,
                          umask_);
  }
}

namespace FUSE
{
  int
  create(const char       *fusepath_,
         mode_t            mode_,
         fuse_file_info_t *ffi_)
  {
    int rv;
    Config::Read cfg;
    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    utils::cfg_to_ffi_flags(cfg,fc->pid,ffi_);

    if(cfg->writeback_cache)
      utils::tweak_flags_writeback_cache(&ffi_->flags);

    ffi_->noflush = !utils::calculate_flush(cfg->flushonclose,
                                            ffi_->flags);

    rv = l::create(cfg->func.getattr.policy,
                   cfg->func.create.policy,
                   cfg->branches,
                   fusepath_,
                   ffi_,
                   mode_,
                   fc->umask);
    if(rv == -EROFS)
      {
        Config::Write()->branches.find_and_set_mode_ro();
        rv = l::create(cfg->func.getattr.policy,
                       cfg->func.create.policy,
                       cfg->branches,
                       fusepath_,
                       ffi_,
                       mode_,
                       fc->umask);
      }

    return rv;
  }
}
