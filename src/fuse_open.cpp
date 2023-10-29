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


namespace l
{
  static
  bool
  rdonly(const int flags_)
  {
    return ((flags_ & O_ACCMODE) == O_RDONLY);
  }

  static
  int
  lchmod_and_open_if_not_writable_and_empty(const std::string &fullpath_,
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
  nfsopenhack(const std::string &fullpath_,
              const int          flags_,
              const NFSOpenHack  nfsopenhack_)
  {
    switch(nfsopenhack_)
      {
      default:
      case NFSOpenHack::ENUM::OFF:
        return (errno=EACCES,-1);
      case NFSOpenHack::ENUM::GIT:
        if(l::rdonly(flags_))
          return (errno=EACCES,-1);
        if(fullpath_.find("/.git/") == std::string::npos)
          return (errno=EACCES,-1);
        return l::lchmod_and_open_if_not_writable_and_empty(fullpath_,flags_);
      case NFSOpenHack::ENUM::ALL:
        if(l::rdonly(flags_))
          return (errno=EACCES,-1);
        return l::lchmod_and_open_if_not_writable_and_empty(fullpath_,flags_);
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
  tweak_flags_writeback_cache(int *flags_)
  {
    if((*flags_ & O_ACCMODE) == O_WRONLY)
      *flags_ = ((*flags_ & ~O_ACCMODE) | O_RDWR);
    if(*flags_ & O_APPEND)
      *flags_ &= ~O_APPEND;
  }

  static
  bool
  calculate_flush(FlushOnClose const flushonclose_,
                  int const          flags_)
  {
    switch(flushonclose_)
      {
      case FlushOnCloseEnum::NEVER:
        return false;
      case FlushOnCloseEnum::OPENED_FOR_WRITE:
        return !l::rdonly(flags_);
      case FlushOnCloseEnum::ALWAYS:
        return true;
      }

    return true;
  }

  static
  void
  config_to_ffi_flags(Config::Read     &cfg_,
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
  open_core(const std::string &basepath_,
            const char        *fusepath_,
            fuse_file_info_t  *ffi_,
            const bool         link_cow_,
            const NFSOpenHack  nfsopenhack_)
  {
    int fd;
    FileInfo *fi;
    std::string fullpath;

    fullpath = fs::path::make(basepath_,fusepath_);

    if(link_cow_ && fs::cow::is_eligible(fullpath.c_str(),ffi_->flags))
      fs::cow::break_link(fullpath.c_str());

    fd = fs::open(fullpath,ffi_->flags);
    if((fd == -1) && (errno == EACCES))
      fd = l::nfsopenhack(fullpath,ffi_->flags,nfsopenhack_);
    if(fd == -1)
      return -errno;

    fi = new FileInfo(fd,fusepath_,ffi_->direct_io);

    ffi_->fh = reinterpret_cast<uint64_t>(fi);

    return 0;
  }

  static
  int
  open(const Policy::Search &searchFunc_,
       const Branches       &branches_,
       const char           *fusepath_,
       fuse_file_info_t     *ffi_,
       const bool            link_cow_,
       const NFSOpenHack     nfsopenhack_)
  {
    int rv;
    StrVec basepaths;

    rv = searchFunc_(branches_,fusepath_,&basepaths);
    if(rv == -1)
      return -errno;

    return l::open_core(basepaths[0],fusepath_,ffi_,link_cow_,nfsopenhack_);
  }
}

namespace FUSE
{
  int
  open(const char       *fusepath_,
       fuse_file_info_t *ffi_)
  {
    int rv;
    Config::Read cfg;
    const fuse_context *fc  = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    l::config_to_ffi_flags(cfg,fc->pid,ffi_);

    if(cfg->writeback_cache)
      l::tweak_flags_writeback_cache(&ffi_->flags);

    ffi_->noflush = !l::calculate_flush(cfg->flushonclose,
                                        ffi_->flags);

    rv = l::open(cfg->func.open.policy,
                 cfg->branches,
                 fusepath_,
                 ffi_,
                 cfg->link_cow,
                 cfg->nfsopenhack);

    return rv;
  }
}
