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

#include "fuse_read_buf.hpp"

#include "fileinfo.hpp"
#include "fs_fstat.hpp"
#include "fs_getfl.hpp"
#include "fs_statx.hpp"
#include "ioprio.hpp"
#include "state.hpp"

#include "fuse.h"

#include "boost/unordered/concurrent_flat_map.hpp"

#include <atomic>
#include <fcntl.h>
#include <shared_mutex>

#if defined(__linux__)
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#endif


/* The branch file's type, size and device. A statx asking only for the
   first two lets filesystems skip work a full fstat implies (NFS
   flushes the file's dirty pages before reporting c/mtime; the device
   is always reported); fstat covers builds and filesystems without
   statx. */
static
int
_type_and_size(const int  fd_,
               mode_t    *mode_,
               s64       *size_,
               dev_t     *dev_)
{
  int rv;
  struct stat st;

#ifdef MERGERFS_SUPPORTED_STATX
  struct fuse_statx stx;
  const unsigned int want = (STATX_TYPE|STATX_SIZE);

  rv = fs::statx(fd_,"",AT_EMPTY_PATH,want,&stx);
  if((rv == 0) && ((stx.mask & want) == want))
    {
      *mode_ = stx.mode;
      *size_ = (s64)stx.size;
      *dev_  = makedev(stx.dev_major,stx.dev_minor);
      return 0;
    }
#endif

  rv = fs::fstat(fd_,&st);
  if(rv < 0)
    return rv;

  *mode_ = st.st_mode;
  *size_ = st.st_size;
  *dev_  = st.st_dev;

  return 0;
}

/* Devices of branch filesystems _splice_coherent refused by type. The
   type is a property of the superblock, not of the open file, and on
   the network and FUSE filesystems it refuses fstatfs is a round trip
   to their server, which the first large read of every opened file
   would otherwise pay. Only refusals are remembered, so a device
   number reused by another filesystem after an unmount can cost
   splicing but never coherence. The value is unused. */
static boost::concurrent_flat_map<dev_t,bool> g_refused_devs;

/* Whether splicing from the branch fd returns what pread would, and is
   worth doing. A filesystem that cannot be identified is refused too.
   Not on FUSE, SMB/CIFS, NFS, Ceph, AFS, overlayfs or 9p
   branches: their splice_read serves the branch inode's page cache
   (filemap_splice_read) while their read path bypasses it or first
   revalidates it (FUSE direct_io and auto_inval_data, SMB without a
   read lease, NFS/Ceph/AFS/9p attribute/data caching, overlayfs's
   copy-up), so a spliced reply could return stale data where the copy
   path returns current data. Nor on an fd opened O_DIRECT, regardless
   of filesystem: the kernel serves that splice by direct I/O into
   freshly allocated pipe pages (as pread does, so nothing is stale),
   which saves no copy over the copy path while adding syscalls, and a
   misaligned request's EINVAL would look like a filesystem without
   splice_read. A refusal by filesystem type is remembered for dev_ (the
   fd's device, 0 if unknown) so later handles on that filesystem skip
   the fstatfs. */
static
bool
_splice_coherent(const int   fd_,
                 const dev_t dev_)
{
#if defined(__linux__)
  constexpr u32 MAGIC_FUSE       = 0x65735546; // fuse, fuseblk, virtiofs
  constexpr u32 MAGIC_CIFS       = 0xFF534D42;
  constexpr u32 MAGIC_SMB2       = 0xFE534D42;
  constexpr u32 MAGIC_NFS        = 0x00006969;
  constexpr u32 MAGIC_CEPH       = 0x00C36400;
  constexpr u32 MAGIC_AFS        = 0x5346414F; // OpenAFS (/afs)
  constexpr u32 MAGIC_KAFS       = 0x6B414653; // in-kernel AFS client
  constexpr u32 MAGIC_OVERLAYFS  = 0x794C7630;
  constexpr u32 MAGIC_V9FS       = 0x01021997;
  struct statfs st;
  int rv;

  if(::fstatfs(fd_,&st) < 0)
    return false;

  switch((u32)st.f_type)
    {
    case MAGIC_FUSE:
    case MAGIC_CIFS:
    case MAGIC_SMB2:
    case MAGIC_NFS:
    case MAGIC_CEPH:
    case MAGIC_AFS:
    case MAGIC_KAFS:
    case MAGIC_OVERLAYFS:
    case MAGIC_V9FS:
      if(dev_ != 0)
        g_refused_devs.emplace(dev_,true);
      return false;
    default:
      break;
    }

  rv = fs::getfl(fd_);
  if((rv >= 0) && (rv & O_DIRECT))
    return false;

  return true;
#else
  (void)fd_;
  (void)dev_;
  return false;
#endif
}

/* Whether [offset, offset+size) lies inside [0, bound), computed
   without overflow: offset+size itself can exceed INT64_MAX for reads
   near the top of a huge sparse file. */
static
inline
bool
_window_inside(const off_t  offset_,
               const size_t size_,
               const s64    bound_)
{
  return ((offset_ >= 0) &&
          (offset_ < bound_) &&
          ((u64)size_ <= (u64)(bound_ - offset_)));
}

/* Describe a large read as a region of the branch file so libfuse can
   splice it into /dev/fuse without the data crossing userspace.
   Returning a positive value declines, and libfuse calls FUSE::read.

   The reply header, whose length is the region size, is staged before
   any data, so only a read lying entirely within the file's reported
   size is offered. A read crossing it declines rather than being
   trimmed: some branches report a size that lags the data
   (attribute-caching network or FUSE filesystems), and a trimmed reply
   would end short where pread returns the rest. This also declines
   non-regular files and files reporting size 0 (procfs) or 4096
   (sysfs) while returning live content, which a splice past the
   reported size would generate twice or block on. A file that shrinks
   between the size check and the splice falls back to FUSE::read.

   The size check is skipped when the window ends inside
   fi->size_floor, a size already observed on this handle; a stale floor
   only costs a fallback, whose short FUSE::read lowers it.

   fi->mutex is held shared so moveonenospc's dup2 cannot rebind fd
   while libfuse stages from it (see FileRegion::lock). It is never
   waited for: a move holds it for a whole file copy, so the read
   declines instead and the copy path serves it from the old fd. */
int
FUSE::read_buf(const fuse_req_ctx_t   *ctx_,
               const fuse_file_info_t *ffi_,
               FileRegion             *region_,
               const size_t            size_,
               const off_t             offset_)
{
  FileInfo *fi;
  FileInfo::Splice splice;
  s64 floor;
  dev_t dev = 0;

  fi = state.get_fi(ctx_,ffi_->fh);
  if(not fi)
    return -EBADF;

  if(fi->splice_state.load(std::memory_order_relaxed) == FileInfo::Splice::no)
    return 1;

  std::shared_lock<std::shared_mutex> lock(fi->mutex,std::try_to_lock);
  if(not lock.owns_lock())
    return 1;

  /* Checked after the lock is held so a mover that started waiting
     before try_to_lock succeeded is not joined by this splice. */
  if(fi->exclusive_waiters.load(std::memory_order_relaxed) > 0)
    return 1;

  /* Read again under the lock: a move that completed before try_to_lock
     rebound fd and reset this to unknown (fd_rebound), and the
     once-per-binding check below must see that. */
  splice = fi->splice_state.load(std::memory_order_relaxed);
  if(splice == FileInfo::Splice::no)
    return 1;

  floor = fi->size_floor.load(std::memory_order_relaxed);
  if(not ::_window_inside(offset_,size_,floor))
    {
      mode_t mode;
      s64    fsize;

      if((::_type_and_size(fi->fd,&mode,&fsize,&dev) < 0) ||
         !S_ISREG(mode))
        return 1;

      /* On a filesystem already refused by type, settle this handle
         now, even for a read that would decline below anyway (every
         read of a file smaller than the request does), so its later
         reads skip this statx. */
      if((splice == FileInfo::Splice::unknown) && g_refused_devs.contains(dev))
        {
          fi->splice_state.compare_exchange_strong(splice,
                                                   FileInfo::Splice::no,
                                                   std::memory_order_relaxed);
          return 1;
        }

      /* Only replace the floor this check started from: a short read
         may have lowered it meanwhile, and that is newer than fsize. */
      if(fsize > 0)
        fi->size_floor.compare_exchange_strong(floor,fsize,
                                               std::memory_order_relaxed);
      if(not ::_window_inside(offset_,size_,fsize))
        return 1;
    }

  /* Once per fd binding, for a read that could otherwise splice, and
     under the lock so the answer is about the file fd refers to. A
     concurrent rejection (read_buf_rejected) wins over this check. */
  if(splice == FileInfo::Splice::unknown)
    {
      /* A filesystem that cannot be identified is not offered either,
         and that is settled too: retrying would repeat the fstatfs,
         possibly a server round trip, on every later large read. */
      const FileInfo::Splice want = (::_splice_coherent(fi->fd,dev) ?
                                     FileInfo::Splice::yes :
                                     FileInfo::Splice::no);

      /* On failure splice holds the concurrent decision, which wins. */
      if(fi->splice_state.compare_exchange_strong(splice,want,
                                                  std::memory_order_relaxed))
        splice = want;
      if(splice != FileInfo::Splice::yes)
        return 1;
    }

  /* ioprio is thread-persistent; without SetFrom the branch-file I/O
     performed downstream of this handler (the kernel-side splice in
     the reply path runs on this same worker thread) would inherit
     whatever priority a previous request left behind. */
  ioprio::SetFrom iop(ctx_->pid);

  region_->fd     = fi->fd;
  region_->offset = offset_;
  region_->size   = size_;
  region_->lock   = std::move(lock);

  return 0;
}

/* The branch fd refused splice (EINVAL on its first splice: its
   filesystem has no splice_read). That will not change for this fd, so
   stop offering it and let every later read go straight to FUSE::read.
   O_DIRECT fds never get here: _splice_coherent refuses them. libfuse
   calls this while the region's lock is still held, so fd is still the
   one that refused. */
void
FUSE::read_buf_rejected(const fuse_req_ctx_t   *ctx_,
                        const fuse_file_info_t *ffi_)
{
  FileInfo *fi;

  fi = state.get_fi(ctx_,ffi_->fh);
  if(not fi)
    return;

  fi->splice_state.store(FileInfo::Splice::no,std::memory_order_relaxed);
}
