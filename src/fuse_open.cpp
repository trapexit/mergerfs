/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

/*
  OPEN FILE MANAGEMENT AND SHARED HANDLE TRACKING
  ===============================================

  This file implements FUSE open() with handling for concurrent access
  to the same file. The key challenge is that multiple threads may
  simultaneously attempt to open the same file (same nodeid), and we
  need to:

  1. Share a single canonical FileInfo entry in the open_files map for all
  opens of the same file while still giving each FUSE handle its own
  FileInfo object and file descriptor
  2. Properly reference count to know when to release resources
  3. Handle race conditions without deadlocks or resource leaks
  4. Support FUSE passthrough mode for performance

  CONCURRENCY MODEL
  -----------------

  The open_files map (in state.hpp) tracks all currently open files:
  - Key: nodeid (FUSE inode number)
  - Value: OpenFile struct { ref_count, backing_id, fi }

  When a file is opened:
  1. First, we check if an entry already exists (using visit())
  2. If it exists, we increment ref_count and create a new per-handle
     FileInfo by re-opening the canonical file descriptor
  3. If it doesn't exist, we create a new FileInfo and try to insert it
  4. If insertion fails (another thread inserted first), we clean up
  and retry (because to use passthrough it MUST be the same underlying
  file as the backing id was created)

  THE RETRY LOOP
  --------------

  The MAX_OPEN_RETRIES loop (lines ~294-380) handles the race where multiple
  threads try to open the same file simultaneously:

  Thread A: visit() -> no entry -> open file -> try_emplace() -> SUCCESS
  Thread B: visit() -> no entry -> open file -> try_emplace() -> FAIL
  Thread B: (cleans up, retries) -> visit() -> found Thread A's entry -> share it

  This ensures that all threads eventually succeed while sharing the same
  underlying file resources. The alternative is to not use passthrough
  on failure but this race should be rare.

  RELEASE INTERACTION
  -------------------

  release() erases the map entry on the final close. Until then, the
  canonical FileInfo in the map remains valid and overlapping opens can bump
  ref_count and duplicate its fd. Secondary handles own their own FileInfo
  objects and are freed independently during release().

  PASSTHROUGH MODE
  ----------------

  FUSE passthrough (Linux 6.9+) allows the kernel to bypass mergerfs for I/O:
  - We request a backing_id from the kernel via fuse_passthrough_open()
  - If successful, subsequent reads/writes go directly to the backing file
  - This achieves near native performance

  The backing_id is stored in the OpenFile entry so all overlapping
  open requests share the same passthrough handle.
*/

#include "fuse_open.hpp"

#include "state.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fuse_release.hpp"
#include "fs_close.hpp"
#include "fs_cow.hpp"
#include "fs_fchmod.hpp"
#include "fs_lchmod.hpp"
#include "fs_open.hpp"
#include "fs_open_fd.hpp"
#include "fs_openat.hpp"
#include "fs_path.hpp"
#include "fs_stat.hpp"
#include "fuse_passthrough.hpp"
#include "procfs.hpp"
#include "stat_util.hpp"

#include "fuse.h"

#include <sched.h>

#include <set>
#include <string>
#include <vector>

#define MAX_OPEN_RETRIES 64


static
bool
_rdonly(const int flags_)
{
  return ((flags_ & O_ACCMODE) == O_RDONLY);
}

static
int
_lchmod_and_open_if_not_writable_and_empty(const fs::path &fullpath_,
                                           const int       flags_)
{
  int rv;
  struct stat st;

  rv = fs::lstat(fullpath_,&st);
  if(rv < 0)
    return -EACCES;

  if(StatUtil::writable(st))
    return -EACCES;

  rv = fs::lchmod(fullpath_,(st.st_mode|S_IWUSR|S_IWGRP));
  if(rv < 0)
    return -EACCES;

  rv = fs::open(fullpath_,flags_);
  if(rv < 0)
    return -EACCES;

  fs::fchmod(rv,st.st_mode);

  return rv;
}

static
int
_nfsopenhack(const fs::path    &fullpath_,
             const int          flags_,
             const NFSOpenHack  nfsopenhack_)
{
  switch(nfsopenhack_)
    {
    default:
    case NFSOpenHack::ENUM::OFF:
      return -EACCES;
    case NFSOpenHack::ENUM::GIT:
      if(::_rdonly(flags_))
        return -EACCES;
      if(fullpath_.string().find("/.git/") == std::string::npos)
        return -EACCES;
      return ::_lchmod_and_open_if_not_writable_and_empty(fullpath_,flags_);
    case NFSOpenHack::ENUM::ALL:
      if(::_rdonly(flags_))
        return -EACCES;
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
_tweak_flags_cache_writeback(int *flags_)
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
_config_to_ffi_flags(const int         tid_,
                     fuse_file_info_t *ffi_)
{
  switch(cfg.cache_files)
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
      if(cfg.cache_files_process_names.count(proc_name) == 0)
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

  if(cfg.parallel_direct_writes == true)
    ffi_->parallel_direct_writes = ffi_->direct_io;
}

static
int
_open_path(const fs::path    &filepath_,
           const Branch      *branch_,
           const fs::path    &fusepath_,
           fuse_file_info_t  *ffi_,
           const NFSOpenHack  nfsopenhack_)
{
  int fd;
  FileInfo *fi;

  fd = fs::openat(AT_FDCWD,filepath_,ffi_->flags);
  if(fd == -EACCES)
    fd = ::_nfsopenhack(filepath_,ffi_->flags,nfsopenhack_);
  if(fd < 0)
    return fd;

  fi = new FileInfo(fd,branch_,fusepath_,ffi_->direct_io);

  ffi_->fh = fi->to_fh();

  return 0;
}

static
int
_open_fd(const int         fd_,
         const Branch     *branch_,
         const fs::path   &fusepath_,
         fuse_file_info_t *ffi_)
{
  int fd;
  FileInfo *fi;

  fd = fs::open_fd(fd_,ffi_->flags);
  if(fd < 0)
    return fd;

  fi = new FileInfo(fd,branch_,fusepath_,ffi_->direct_io);

  ffi_->fh = fi->to_fh();

  return 0;
}

static
int
_open(const Policy::Search &searchFunc_,
      const Branches       &ibranches_,
      const fs::path       &fusepath_,
      fuse_file_info_t     *ffi_,
      const bool            link_cow_,
      const NFSOpenHack     nfsopenhack_)
{
  int rv;
  fs::path filepath;
  std::vector<Branch*> obranches;

  rv = searchFunc_(ibranches_,fusepath_,obranches);
  if(rv < 0)
    return rv;

  filepath = obranches[0]->path / fusepath_;

  if(link_cow_ && fs::cow::is_eligible(filepath,ffi_->flags))
    fs::cow::break_link(filepath);

  rv = ::_open_path(filepath,
                    obranches[0],
                    fusepath_,
                    ffi_,
                    nfsopenhack_);

  return rv;
}

constexpr
u64
_(const PassthroughIOEnum e_,
  const u64                m_)
{
  return ((((u64)e_) << 32) | (m_ & O_ACCMODE));
}

static
int
_open(const fuse_req_ctx_t *ctx_,
      const fs::path       &fusepath_,
      fuse_file_info_t     *ffi_)
{
  int rv;
  auto &of = state.open_files;

  ::_config_to_ffi_flags(ctx_->pid,ffi_);
  if(cfg.cache_writeback)
    ::_tweak_flags_cache_writeback(&ffi_->flags);
  ffi_->noflush = !::_calculate_flush(cfg.flushonclose,ffi_->flags);

  for(int i = 0; i < MAX_OPEN_RETRIES; i++)
    {
      FileInfo *fi = nullptr;
      int backing_id = INVALID_BACKING_ID;

      // The ref increment keeps fuse_release.cpp from erasing and
      // freeing the canonical entry while we duplicate its fd.
      of.visit(ctx_->nodeid,
               [&](auto &v_)
               {
                 v_.second.ref_count++;
                 fi = v_.second.fi;
                 backing_id = v_.second.backing_id;
               });

      // If the file is already open...
      if(fi)
        {
          rv = ::_open_fd(fi->fd,
                          &fi->branch,
                          fusepath_,
                          ffi_);

          // If we fail to reopen the already open file we need to
          // treat it similarly to fuse_release. Since we increased
          // the ref count without an actual new fileinfo obj we call
          // FUSE::release() without an obj. Nothing was allocated
          // into ffi_->fh. _open_fd() failing should really never
          // happen hence no retries. Just being careful.
          if(rv < 0)
            {
              FUSE::release(ctx_->nodeid);
              return rv;
            }

          switch(_(cfg.passthrough_io,ffi_->flags))
            {
            case _(PassthroughIO::ENUM::RO,O_RDONLY):
            case _(PassthroughIO::ENUM::WO,O_WRONLY):
            case _(PassthroughIO::ENUM::RW,O_RDONLY):
            case _(PassthroughIO::ENUM::RW,O_WRONLY):
            case _(PassthroughIO::ENUM::RW,O_RDWR):
              if(not fuse_backing_id_is_valid(backing_id))
                return 0;
              ffi_->backing_id  = backing_id;
              ffi_->passthrough = true;
              ffi_->keep_cache  = false;
              return 0;
            }

          return 0;
        }

      // Was not open, do first open, try to insert, if someone beat us
      // to it in another thread then throw it away and try again.
      rv = ::_open(cfg.func.open.policy,
                   cfg.branches,
                   fusepath_,
                   ffi_,
                   cfg.link_cow,
                   cfg.nfsopenhack);
      if(rv < 0)
        return rv;

      fi = FileInfo::from_fh(ffi_->fh);
      if(not fi)
        return -EBADF;

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
              ffi_->backing_id  = backing_id;
              ffi_->passthrough = true;
              ffi_->keep_cache  = false;
            }
          break;
        default:
          break;
        }

      bool inserted = of.try_emplace(ctx_->nodeid,
                                     backing_id,
                                     fi);
      if(inserted)
        return 0;

      // Failed to insert because another thread beat us to
      // it. Cleanup the resources and try it all again.
      FUSE::passthrough_close(backing_id);
      fs::close(fi->fd);
      delete fi;
      ffi_->fh = 0;

      struct timespec duration = {0,((i+1)*1000)};
      ::nanosleep(&duration,NULL);
    }

  // Perhaps not the best error to return but it is EXTREMELY unlikely
  // to be hit and if so the system is probably under a lot of
  // stress. Give the app the opportunity to try it again. If it can't
  // it'll error probably similarly as having had returned EIO or
  // similar.
  return -EINTR;
}


int
FUSE::open(const fuse_req_ctx_t *ctx_,
           const char           *fusepath_,
           fuse_file_info_t     *ffi_)
{
  const fs::path fusepath{fusepath_};

  return ::_open(ctx_,fusepath,ffi_);
}
