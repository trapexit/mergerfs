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

#include "state.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_acl.hpp"
#include "fs_clonepath.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "fuse_passthrough.hpp"
#include "procfs.hpp"
#include "ugid.hpp"

#include "fuse.h"

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
_tweak_flags_writeback_cache(int *flags_)
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
_config_to_ffi_flags(Config::Read     &cfg_,
                     const int         tid_,
                     fuse_file_info_t *ffi_)
{
  switch(cfg_->cache_files)
    {
    case CacheFiles::ENUM::LIBFUSE:
      ffi_->direct_io  = cfg_->direct_io;
      ffi_->keep_cache = cfg_->kernel_cache;
      ffi_->auto_cache = cfg_->auto_cache;
      break;
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
      if(cfg_->cache_files_process_names.count(proc_name) == 0)
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

  if(cfg_->parallel_direct_writes == true)
    ffi_->parallel_direct_writes = ffi_->direct_io;
}

static
int
_create_core(const std::string &fullpath_,
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
_create_core(const Branch     *branch_,
             const char       *fusepath_,
             fuse_file_info_t *ffi_,
             const mode_t      mode_,
             const mode_t      umask_)
{
  int rv;
  FileInfo *fi;
  std::string fullpath;

  fullpath = fs::path::make(branch_->path,fusepath_);

  rv = ::_create_core(fullpath,mode_,umask_,ffi_->flags);
  if(rv == -1)
    return -errno;

  fi = new FileInfo(rv,branch_,fusepath_,ffi_->direct_io);

  ffi_->fh = reinterpret_cast<uint64_t>(fi);

  return 0;
}

static
int
_create(const Policy::Search &searchFunc_,
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
  std::vector<Branch*> createpaths;
  std::vector<Branch*> existingpaths;

  fusedirpath = fs::path::dirname(fusepath_);

  rv = searchFunc_(branches_,fusedirpath,existingpaths);
  if(rv == -1)
    return -errno;

  rv = createFunc_(branches_,fusedirpath,createpaths);
  if(rv == -1)
    return -errno;

  rv = fs::clonepath_as_root(existingpaths[0]->path,
                             createpaths[0]->path,
                             fusedirpath);
  if(rv == -1)
    return -errno;

  return ::_create_core(createpaths[0],
                        fusepath_,
                        ffi_,
                        mode_,
                        umask_);
}

constexpr
const
uint64_t
_(const PassthroughEnum e_,
  const uint64_t        m_)
{
  return ((((uint64_t)e_) << 32) | (m_ & O_ACCMODE));
}

static
int
_create_for_insert_lambda(const fuse_context *fc_,
                          const char         *fusepath_,
                          const mode_t        mode_,
                          fuse_file_info_t   *ffi_,
                          State::OpenFile    *of_)
{
  int rv;
  FileInfo *fi;
  Config::Read cfg;
  const ugid::Set ugid(fc_->uid,fc_->gid);

  ::_config_to_ffi_flags(cfg,fc_->pid,ffi_);
  if(cfg->writeback_cache)
    ::_tweak_flags_writeback_cache(&ffi_->flags);
  ffi_->noflush = !::_calculate_flush(cfg->flushonclose,
                                      ffi_->flags);

  rv = ::_create(cfg->func.getattr.policy,
                 cfg->func.create.policy,
                 cfg->branches,
                 fusepath_,
                 ffi_,
                 mode_,
                 fc_->umask);
  if(rv == -EROFS)
    {
      Config::Write()->branches.find_and_set_mode_ro();
      rv = ::_create(cfg->func.getattr.policy,
                     cfg->func.create.policy,
                     cfg->branches,
                     fusepath_,
                     ffi_,
                     mode_,
                     fc_->umask);
    }

  if(rv < 0)
    return rv;

  fi = reinterpret_cast<FileInfo*>(ffi_->fh);

  of_->ref_count = 1;
  of_->fi        = fi;

  switch(_(cfg->passthrough,ffi_->flags))
    {
    case _(Passthrough::ENUM::RO,O_RDONLY):
    case _(Passthrough::ENUM::WO,O_WRONLY):
    case _(Passthrough::ENUM::RW,O_RDONLY):
    case _(Passthrough::ENUM::RW,O_WRONLY):
    case _(Passthrough::ENUM::RW,O_RDWR):
      break;
    default:
      return 0;
    }

  of_->backing_id = FUSE::passthrough_open(fc_,fi->fd);
  if(of_->backing_id < 0)
    return 0;

  ffi_->backing_id  = of_->backing_id;
  ffi_->passthrough = true;
  ffi_->keep_cache  = false;

  return 0;
}

static
inline
constexpr
auto
_create_insert_lambda(const fuse_context *fc_,
                      const char         *fusepath_,
                      const mode_t        mode_,
                      fuse_file_info_t   *ffi_,
                      int                *_rv_)
{
  return
    [=](auto &val_)
    {
      *_rv_ = ::_create_for_insert_lambda(fc_,
                                          fusepath_,
                                          mode_,
                                          ffi_,
                                          &val_.second);
    };
}

// This function should never be called?
static
inline
constexpr
auto
_create_update_lambda()
{
  return
    [=](auto &val_)
    {
      fmt::print(stderr,"THIS SHOULD NOT HAPPEN");
      abort();
    };
}

static
int
_create(const fuse_context *fc_,
        const char         *fusepath_,
        mode_t              mode_,
        fuse_file_info_t   *ffi_)
{
  int rv;
  auto &of = state.open_files;

  rv = -EINVAL;
  of.try_emplace_and_visit(fc_->nodeid,
                           ::_create_insert_lambda(fc_,fusepath_,mode_,ffi_,&rv),
                           ::_create_update_lambda());

  // Can't abort an emplace_and_visit and can't assume another thread
  // hasn't created an entry since this failure so erase only if
  // ref_count is default (0).
  if(rv < 0)
    of.erase_if(fc_->nodeid,
                [](auto &val_)
                {
                  return (val_.second.ref_count <= 0);
                });

  return rv;
}

namespace FUSE
{
  int
  create(const char       *fusepath_,
         mode_t            mode_,
         fuse_file_info_t *ffi_)
  {
    Config::Read cfg;
    const fuse_context *fc = fuse_get_context();

    return ::_create(fc,fusepath_,mode_,ffi_);
  }
}
