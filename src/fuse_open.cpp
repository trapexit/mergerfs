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
#include "fs_cow.hpp"
#include "fs_fchmod.hpp"
#include "fs_lchmod.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "fs_stat.hpp"
#include "procfs_get_name.hpp"
#include "stat_util.hpp"
#include "ugid.hpp"

#include "fuse.h"

#include <set>
#include <string>
#include <vector>

static
bool
_rdonly(const int flags_)
{
  return ((flags_ & O_ACCMODE) == O_RDONLY);
}

static
int
_lchmod_and_open_if_not_writable_and_empty(const std::string &fullpath_,
                                           const int          flags_)
{
  int rv;
  struct stat st;

  rv = fs::lstat(fullpath_,&st);
  if(rv == -1)
    return (errno=EACCES,-1);

  if(StatUtil::writable(st))
    return (errno=EACCES,-1);

  rv = fs::lchmod(fullpath_,(st.st_mode|S_IWUSR|S_IWGRP));
  if(rv == -1)
    return (errno=EACCES,-1);

  rv = fs::open(fullpath_,flags_);
  if(rv == -1)
    return (errno=EACCES,-1);

  fs::fchmod(rv,st.st_mode);

  return rv;
}

static
int
_nfsopenhack(const std::string &fullpath_,
             const int          flags_,
             const NFSOpenHack  nfsopenhack_)
{
  switch(nfsopenhack_)
    {
    default:
    case NFSOpenHack::ENUM::OFF:
      return (errno=EACCES,-1);
    case NFSOpenHack::ENUM::GIT:
      if(::_rdonly(flags_))
        return (errno=EACCES,-1);
      if(fullpath_.find("/.git/") == std::string::npos)
        return (errno=EACCES,-1);
      return ::_lchmod_and_open_if_not_writable_and_empty(fullpath_,flags_);
    case NFSOpenHack::ENUM::ALL:
      if(::_rdonly(flags_))
        return (errno=EACCES,-1);
      return ::_lchmod_and_open_if_not_writable_and_empty(fullpath_,flags_);
    }
}

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
_open_core(const Branch      *branch_,
           const char        *fusepath_,
           fuse_file_info_t  *ffi_,
           const bool         link_cow_,
           const NFSOpenHack  nfsopenhack_)
{
  int fd;
  FileInfo *fi;
  std::string fullpath;

  fullpath = fs::path::make(branch_->path,fusepath_);

  if(link_cow_ && fs::cow::is_eligible(fullpath.c_str(),ffi_->flags))
    fs::cow::break_link(fullpath.c_str());

  fd = fs::open(fullpath,ffi_->flags);
  if((fd == -1) && (errno == EACCES))
    fd = ::_nfsopenhack(fullpath,ffi_->flags,nfsopenhack_);
  if(fd == -1)
    return -errno;

  fi = new FileInfo(fd,branch_,fusepath_,ffi_->direct_io);

  ffi_->fh = reinterpret_cast<uint64_t>(fi);

  return 0;
}

static
int
_open(const Policy::Search &searchFunc_,
      const Branches       &ibranches_,
      const char           *fusepath_,
      fuse_file_info_t     *ffi_,
      const bool            link_cow_,
      const NFSOpenHack     nfsopenhack_)
{
  int rv;
  std::vector<Branch*> obranches;

  rv = searchFunc_(ibranches_,fusepath_,obranches);
  if(rv == -1)
    return -errno;

  rv = ::_open_core(obranches[0],
                    fusepath_,
                    ffi_,
                    link_cow_,
                    nfsopenhack_);

  return rv;
}

static
int
_open(const fuse_context *fc_,
      const char         *fusepath_,
      fuse_file_info_t   *ffi_)
{
  int rv;
  Config::Read cfg;
  const ugid::Set ugid(fc_->uid,fc_->gid);

  ::_config_to_ffi_flags(cfg,fc_->pid,ffi_);

  if(cfg->writeback_cache)
    ::_tweak_flags_writeback_cache(&ffi_->flags);

  ffi_->noflush = !::_calculate_flush(cfg->flushonclose,
                                      ffi_->flags);

  rv = ::_open(cfg->func.open.policy,
               cfg->branches,
               fusepath_,
               ffi_,
               cfg->link_cow,
               cfg->nfsopenhack);

  return rv;
}

static
inline
constexpr
auto
_open_passthrough_insert_lambda(const fuse_context *fc_,
                                const char         *fusepath_,
                                fuse_file_info_t   *ffi_,
                                int                &rv_)
{
  return
    [=,&rv_](auto &val)
    {
      FileInfo *fi;

      throw std::runtime_error("foo");

      rv_ = ::_open(fc_,fusepath_,ffi_);
      if(rv_ < 0)
        return;

      fi = reinterpret_cast<FileInfo*>(ffi_->fh);

      val.second.ref_count = 1;
      val.second.fi        = fi;

      int rv;
      const ugid::SetRootGuard ugid;

      rv = fuse_passthrough_open(fc_,fi->fd);
      if(rv >= 0)
        {
          ffi_->passthrough     = true;
          ffi_->backing_id      = rv;
          val.second.backing_id = rv;
          ffi_->keep_cache      = false;
        }
    };
}

static
inline
constexpr
auto
_open_passthrough_update_lambda(const fuse_context *fc_,
                                const char         *fusepath_,
                                fuse_file_info_t   *ffi_,
                                int                *rv_)
{
  return
    [=](auto &val)
    {
      *rv_ = -EINVAL;
    };
}

static
int
_open_passthrough(const fuse_context *fc_,
                  const char         *fusepath_,
                  fuse_file_info_t   *ffi_)
{
  int rv;
  auto &pt = state.passthrough;

  rv = -EINVAL;
  pt.try_emplace_and_visit(fusepath_,
                           ::_open_passthrough_insert_lambda(fc_,fusepath_,ffi_,rv),
                           ::_open_passthrough_update_lambda(fc_,fusepath_,ffi_,rv));

  // Can't abort an emplace_and_visit and can't assume another thread
  // hasn't created an entry since this failure so erase only if
   // ref_count is default.
  if(rv < 0)
    pt.erase_if(fusepath_,
                [](auto &val)
                {
                  return (val.second.ref_count <= 0);
                });
  return rv;
}



namespace FUSE
{
  int
  open(const char       *fusepath_,
       fuse_file_info_t *ffi_)
  {
    Config::Read cfg;
    const fuse_context *fc = fuse_get_context();

    if(cfg->passthrough)
      return ::_open_passthrough(fc,fusepath_,ffi_);

    return ::_open(fc,fusepath_,ffi_);
  }
}
