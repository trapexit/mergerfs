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

#include "fuse_create.hpp"

#include "state.hpp"
#include "config.hpp"

#include "fs_readlink.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_acl.hpp"
#include "fs_close.hpp"
#include "fs_clonepath.hpp"
#include "fs_open.hpp"
#include "fs_open_as.hpp"
#include "fs_openat.hpp"
#include "fs_path.hpp"
#include "fuse_passthrough.hpp"
#include "procfs.hpp"
#include "syslog.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <cassert>
#include <string>
#include <vector>

/*
  The kernel expects being able to issue read requests when running
  with writeback caching enabled so we must change O_WRONLY to
  O_RDWR.

  With writeback caching enabled the kernel handles O_APPEND. Could
  be an issue if the underlying file changes out of band but that is
  true of any caching.
*/
static
void
_tweak_flags_cache_writeback(int *flags_)
{
  if((*flags_ & O_ACCMODE) == O_WRONLY)
    *flags_ = ((*flags_ & ~O_ACCMODE) | O_RDWR);
  if(*flags_ & O_APPEND)
    *flags_ &= ~O_APPEND;
}

static
bool
_rdonly(const int flags_)
{
  return ((flags_ & O_ACCMODE) == O_RDONLY);
}

static
bool
_calculate_flush(FlushOnClose const flushonclose_,
                 int const          flags_)
{
  switch(flushonclose_)
    {
    case FlushOnCloseEnum::NEVER:
      return false;
    case FlushOnCloseEnum::OPENED_FOR_WRITE:
      return !::_rdonly(flags_);
    case FlushOnCloseEnum::ALWAYS:
      return true;
    }

  return true;
}

static
void
_config_to_ffi_flags(Config           &cfg_,
                     const int         tid_,
                     fuse_file_info_t *ffi_)
{
  switch(cfg_.cache_files)
    {
    case CacheFiles::ENUM::OFF:
      ffi_->direct_io  = 1;
      ffi_->keep_cache = 0;
      ffi_->auto_cache = 0;
      break;
    case CacheFiles::ENUM::PARTIAL:
      ffi_->direct_io  = 0;
      ffi_->keep_cache = 0;
      ffi_->auto_cache = 0;
      break;
    case CacheFiles::ENUM::FULL:
      ffi_->direct_io  = 0;
      ffi_->keep_cache = 1;
      ffi_->auto_cache = 0;
      break;
    case CacheFiles::ENUM::AUTO_FULL:
      ffi_->direct_io  = 0;
      ffi_->keep_cache = 0;
      ffi_->auto_cache = 1;
      break;
    case CacheFiles::ENUM::PER_PROCESS:
      std::string proc_name;

      proc_name = procfs::get_name(tid_);
      if(cfg_.cache_files_process_names.count(proc_name) == 0)
        {
          ffi_->direct_io  = 1;
          ffi_->keep_cache = 0;
          ffi_->auto_cache = 0;
        }
      else
        {
          ffi_->direct_io  = 0;
          ffi_->keep_cache = 0;
          ffi_->auto_cache = 0;
        }
      break;
    }

  if(cfg_.parallel_direct_writes == true)
    ffi_->parallel_direct_writes = ffi_->direct_io;
}

static
int
_create_core(const ugid_t    ugid_,
             const fs::path &fullpath_,
             mode_t          mode_,
             const mode_t    umask_,
             const int       flags_)
{
  if(!fs::acl::dir_has_defaults(fullpath_))
    mode_ &= ~umask_;

  return fs::open_as(ugid_,fullpath_,flags_,mode_);
}

static
int
_create_core(const ugid_t      ugid_,
             const Branch     *branch_,
             const fs::path   &fusepath_,
             fuse_file_info_t *ffi_,
             const mode_t      mode_,
             const mode_t      umask_)
{
  int rv;
  FileInfo *fi;
  fs::path fullpath;

  fullpath = branch_->path / fusepath_;

  rv = ::_create_core(ugid_,fullpath,mode_,umask_,ffi_->flags);
  if(rv < 0)
    return rv;

  fi = new FileInfo(rv,*branch_,fusepath_,ffi_->direct_io);

  ffi_->fh = fi->to_fh();

  return 0;
}

static
int
_create(const ugid_t          ugid_,
        const Policy::Search &searchFunc_,
        const Policy::Create &createFunc_,
        const Branches       &branches_,
        const fs::path       &fusepath_,
        fuse_file_info_t     *ffi_,
        const mode_t          mode_,
        const mode_t          umask_)
{
  int rv;
  fs::path fullpath;
  fs::path fusedirpath;
  std::vector<Branch*> createpaths;
  std::vector<Branch*> existingpaths;

  fusedirpath = fusepath_.parent_path();

  rv = searchFunc_(branches_,fusedirpath,existingpaths);
  if(rv < 0)
    return rv;

  rv = createFunc_(branches_,fusedirpath,createpaths);
  if(rv < 0)
    return rv;

  rv = fs::clonepath(existingpaths[0]->path,
                     createpaths[0]->path,
                     fusedirpath);
  if(rv < 0)
    return rv;

  return ::_create_core(ugid_,
                        createpaths[0],
                        fusepath_,
                        ffi_,
                        mode_,
                        umask_);
}

constexpr
uint64_t
_(const PassthroughIOEnum e_,
  const uint64_t          m_)
{
  return ((((uint64_t)e_) << 32) | (m_ & O_ACCMODE));
}

static
int
_create(const fuse_req_ctx_t *ctx_,
        const fs::path       &fusepath_,
        mode_t                mode_,
        fuse_file_info_t     *ffi_)
{
  auto &of = state.open_files;

  ::_config_to_ffi_flags(cfg,ctx_->pid,ffi_);
  if(cfg.cache_writeback)
    ::_tweak_flags_cache_writeback(&ffi_->flags);
  ffi_->noflush = !::_calculate_flush(cfg.flushonclose,ffi_->flags);

  int rv = ::_create(ctx_,
                     cfg.func.getattr.policy,
                     cfg.func.create.policy,
                     cfg.branches,
                     fusepath_,
                     ffi_,
                     mode_,
                     ctx_->umask);
  if(rv == -EROFS)
    {
      cfg.branches.find_and_set_mode_ro();
      rv = ::_create(ctx_,
                     cfg.func.getattr.policy,
                     cfg.func.create.policy,
                     cfg.branches,
                     fusepath_,
                     ffi_,
                     mode_,
                     ctx_->umask);
    }

  if(rv < 0)
    return rv;

  FileInfo *fi = FileInfo::from_fh(ffi_->fh);
  int backing_id = INVALID_BACKING_ID;

  switch(_(cfg.passthrough_io,ffi_->flags))
    {
    case _(PassthroughIO::ENUM::RO,O_RDONLY):
    case _(PassthroughIO::ENUM::WO,O_WRONLY):
    case _(PassthroughIO::ENUM::RW,O_RDONLY):
    case _(PassthroughIO::ENUM::RW,O_WRONLY):
    case _(PassthroughIO::ENUM::RW,O_RDWR):
      backing_id = FUSE::passthrough_open(fi->fd);
      if(fuse_backing_id_is_valid(backing_id))
        {
          ffi_->backing_id = backing_id;
          ffi_->passthrough = true;
          ffi_->keep_cache = false;
        }
      break;
    default:
      break;
    }

  bool inserted = of.try_emplace(ctx_->nodeid,
                                 backing_id,
                                 fi);

  // Legit... this should never happen.
  assert(inserted);
  (void)inserted;

  return 0;
}

int
FUSE::create(const fuse_req_ctx_t *ctx_,
             const char           *fusepath_,
             mode_t                mode_,
             fuse_file_info_t     *ffi_)
{
  const fs::path fusepath{fusepath_};

  return ::_create(ctx_,fusepath,mode_,ffi_);
}
