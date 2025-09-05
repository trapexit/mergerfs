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

#include "fuse_ioctl.hpp"

#include "fuse_getxattr.hpp"
#include "fuse_setxattr.hpp"

#include "config.hpp"
#include "dirinfo.hpp"
#include "endian.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_close.hpp"
#include "fs_findallfiles.hpp"
#include "fs_ioctl.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "gidcache.hpp"
#include "mergerfs_ioctl.hpp"
#include "str.hpp"
#include "ugid.hpp"

#include <string>
#include <vector>

#include <fcntl.h>
#include <string.h>

// From linux/btrfs.h
#define BTRFS_IOCTL_MAGIC 0x94

#ifndef FS_IOC_GETFLAGS
# define FS_IOC_GETFLAGS _IOR('f',1,long)
#endif

#ifndef FS_IOC_SETFLAGS
# define FS_IOC_SETFLAGS _IOW('f',2,long)
#endif

#ifndef FS_IOC_GETVERSION
# define FS_IOC_GETVERSION _IOR('v',1,long)
#endif

#ifndef FS_IOC_SETVERSION
# define FS_IOC_SETVERSION _IOW('v',2,long)
#endif

/*
  There is a bug with FUSE and these ioctl commands. The regular
  libfuse high level API assumes the output buffer size based on the
  command and gives no control over this. FS_IOC_GETFLAGS and
  FS_IOC_SETFLAGS however are defined as `long` when in fact it is an
  `int`. On 64bit systems where long is 8 bytes this can lead to
  libfuse telling the kernel to write 8 bytes and if the user only
  allocated an integer then it will overwrite the 4 bytes after the
  variable which could result in data corruption and/or crashes.

  I've modified the API to allow changing of the output buffer
  size. This fixes the issue on little endian systems because the
  lower 4 bytes are the same regardless of what the user
  allocated. However, on big endian systems that's not the case and it
  is not possible to safely handle the situation.

  https://lwn.net/Articles/575846/
*/

static
int
_ioctl(const int       fd_,
       const uint32_t  cmd_,
       void           *data_,
       uint32_t       *out_bufsz_)
{
  int rv;

  switch(cmd_)
    {
    case FS_IOC_GETFLAGS:
    case FS_IOC_SETFLAGS:
    case FS_IOC_GETVERSION:
    case FS_IOC_SETVERSION:
      if(endian::is_big() && (sizeof(long) != sizeof(int)))
        return -ENOTTY;
      if((data_ != NULL) && (*out_bufsz_ > 4))
        *out_bufsz_ = 4;
      break;
    }

  rv = fs::ioctl(fd_,cmd_,data_);

  return rv;
}

static
int
_ioctl_file(const fuse_file_info_t *ffi_,
            const uint32_t          cmd_,
            void                   *data_,
            uint32_t               *out_bufsz_)
{
  FileInfo           *fi = reinterpret_cast<FileInfo*>(ffi_->fh);
  const fuse_context *fc = fuse_get_context();
  const ugid::Set     ugid(fc->uid,fc->gid);

  return ::_ioctl(fi->fd,cmd_,data_,out_bufsz_);
}

#ifndef O_NOATIME
#define O_NOATIME 0
#endif

static
int
_ioctl_dir_base(const Policy::Search &searchFunc_,
                const Branches       &branches_,
                const char           *fusepath_,
                const uint32_t        cmd_,
                void                 *data_,
                uint32_t             *out_bufsz_)
{
  int fd;
  int rv;
  std::string fullpath;
  std::vector<Branch*> branches;

  rv = searchFunc_(branches_,fusepath_,branches);
  if(rv < 0)
    return rv;

  fullpath = fs::path::make(branches[0]->path,fusepath_);

  fd = fs::open(fullpath,O_RDONLY|O_NOATIME|O_NONBLOCK);
  if(fd < 0)
    return fd;

  rv = ::_ioctl(fd,cmd_,data_,out_bufsz_);

  fs::close(fd);

  return rv;
}

static
int
_ioctl_dir(const fuse_file_info_t *ffi_,
           const uint32_t          cmd_,
           void                   *data_,
           uint32_t               *out_bufsz_)
{
  DirInfo            *di = reinterpret_cast<DirInfo*>(ffi_->fh);
  const fuse_context *fc = fuse_get_context();
  const ugid::Set     ugid(fc->uid,fc->gid);

  return ::_ioctl_dir_base(cfg.func.open.policy,
                           cfg.branches,
                           di->fusepath.c_str(),
                           cmd_,
                           data_,
                           out_bufsz_);
}

static
bool
_is_btrfs_ioctl_cmd(const unsigned long cmd_)
{
  return (_IOC_TYPE(cmd_) == BTRFS_IOCTL_MAGIC);
}

int
FUSE::ioctl(const fuse_file_info_t *ffi_,
            unsigned long           cmd_,
            void                   *arg_,
            unsigned int            flags_,
            void                   *data_,
            uint32_t               *out_bufsz_)
{
  if(::_is_btrfs_ioctl_cmd(cmd_))
    return -ENOTTY;

  if(flags_ & FUSE_IOCTL_DIR)
    return ::_ioctl_dir(ffi_,cmd_,data_,out_bufsz_);

  return ::_ioctl_file(ffi_,cmd_,data_,out_bufsz_);
}
