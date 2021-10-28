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
#include "dirinfo.hpp"
#include "endian.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_close.hpp"
#include "fs_findallfiles.hpp"
#include "fs_ioctl.hpp"
#include "fs_open.hpp"
#include "fs_path.hpp"
#include "str.hpp"
#include "ugid.hpp"

#include <string>
#include <vector>

#include <fcntl.h>
#include <string.h>

using std::string;
using std::vector;

#ifndef _IOC_TYPE
#define _IOC_TYPE(X) (((X) >> 8) & 0xFF)
#endif

typedef char IOCTL_BUF[4096];
#define IOCTL_APP_TYPE  0xDF
#define IOCTL_FILE_INFO _IOWR(IOCTL_APP_TYPE,0,IOCTL_BUF)

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

namespace l
{
  static
  int
  ioctl(const int       fd_,
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

    return ((rv == -1) ? -errno : rv);
  }

  static
  int
  ioctl_file(const fuse_file_info_t *ffi_,
             const uint32_t          cmd_,
             void                   *data_,
             uint32_t               *out_bufsz_)
  {
    FileInfo           *fi = reinterpret_cast<FileInfo*>(ffi_->fh);
    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::ioctl(fi->fd,cmd_,data_,out_bufsz_);
  }

#ifndef O_NOATIME
#define O_NOATIME 0
#endif

  static
  int
  ioctl_dir_base(const Policy::Search &searchFunc_,
                 const Branches       &branches_,
                 const char           *fusepath_,
                 const uint32_t        cmd_,
                 void                 *data_,
                 uint32_t             *out_bufsz_)
  {
    int fd;
    int rv;
    string fullpath;
    StrVec basepaths;

    rv = searchFunc_(branches_,fusepath_,&basepaths);
    if(rv == -1)
      return -errno;

    fullpath = fs::path::make(basepaths[0],fusepath_);

    fd = fs::open(fullpath,O_RDONLY|O_NOATIME|O_NONBLOCK);
    if(fd == -1)
      return -errno;

    rv = l::ioctl(fd,cmd_,data_,out_bufsz_);

    fs::close(fd);

    return rv;
  }

  static
  int
  ioctl_dir(const fuse_file_info_t *ffi_,
            const uint32_t          cmd_,
            void                   *data_,
            uint32_t               *out_bufsz_)
  {
    Config::Read        cfg;
    DirInfo            *di = reinterpret_cast<DirInfo*>(ffi_->fh);
    const fuse_context *fc = fuse_get_context();
    const ugid::Set     ugid(fc->uid,fc->gid);

    return l::ioctl_dir_base(cfg->func.open.policy,
                             cfg->branches,
                             di->fusepath.c_str(),
                             cmd_,
                             data_,
                             out_bufsz_);
  }

  static
  int
  strcpy(const std::string &s_,
         void              *data_)
  {
    char *data = (char*)data_;

    if(s_.size() >= (sizeof(IOCTL_BUF) - 1))
      return -ERANGE;

    memcpy(data,s_.c_str(),s_.size());
    data[s_.size()] = '\0';

    return s_.size();
  }

  static
  int
  file_basepath(const Policy::Search &searchFunc_,
                const Branches       &branches_,
                const char           *fusepath_,
                void                 *data_)
  {
    int rv;
    StrVec basepaths;

    rv = searchFunc_(branches_,fusepath_,&basepaths);
    if(rv == -1)
      return -errno;

    return l::strcpy(basepaths[0],data_);
  }

  static
  int
  file_basepath(const fuse_file_info_t *ffi_,
                void                   *data_)
  {
    Config::Read cfg;
    std::string  &fusepath = reinterpret_cast<FH*>(ffi_->fh)->fusepath;

    return l::file_basepath(cfg->func.open.policy,
                            cfg->branches,
                            fusepath.c_str(),
                            data_);
  }

  static
  int
  file_relpath(const fuse_file_info_t *ffi_,
               void                   *data_)
  {
    std::string &fusepath = reinterpret_cast<FH*>(ffi_->fh)->fusepath;

    return l::strcpy(fusepath,data_);
  }

  static
  int
  file_fullpath(const Policy::Search &searchFunc_,
                const Branches       &branches_,
                const string         &fusepath_,
                void                 *data_)
  {
    int rv;
    string fullpath;
    StrVec basepaths;

    rv = searchFunc_(branches_,fusepath_,&basepaths);
    if(rv == -1)
      return -errno;

    fullpath = fs::path::make(basepaths[0],fusepath_);

    return l::strcpy(fullpath,data_);
  }

  static
  int
  file_fullpath(const fuse_file_info_t *ffi_,
                void                   *data_)
  {
    Config::Read cfg;
    std::string  &fusepath = reinterpret_cast<FH*>(ffi_->fh)->fusepath;

    return l::file_fullpath(cfg->func.open.policy,
                            cfg->branches,
                            fusepath,
                            data_);
  }

  static
  int
  file_allpaths(const fuse_file_info_t *ffi_,
                void                   *data_)
  {
    Config::Read cfg;
    string concated;
    StrVec paths;
    StrVec branches;
    string &fusepath = reinterpret_cast<FH*>(ffi_->fh)->fusepath;

    cfg->branches->to_paths(branches);

    fs::findallfiles(branches,fusepath.c_str(),&paths);

    concated = str::join(paths,'\0');

    return l::strcpy(concated,data_);
  }

  static
  int
  file_info(const fuse_file_info_t *ffi_,
            void                   *data_)
  {
    char *key = (char*)data_;

    if(!strcmp("basepath",key))
      return l::file_basepath(ffi_,data_);
    if(!strcmp("relpath",key))
      return l::file_relpath(ffi_,data_);
    if(!strcmp("fullpath",key))
      return l::file_fullpath(ffi_,data_);
    if(!strcmp("allpaths",key))
      return l::file_allpaths(ffi_,data_);

    return -ENOATTR;
  }

  static
  bool
  is_mergerfs_ioctl_cmd(const unsigned long cmd_)
  {
    return (_IOC_TYPE(cmd_) == IOCTL_APP_TYPE);
  }

  static
  int
  ioctl_custom(const fuse_file_info_t *ffi_,
               unsigned long           cmd_,
               void                   *data_)
  {
    switch(cmd_)
      {
      case IOCTL_FILE_INFO:
        return l::file_info(ffi_,data_);
      }

    return -ENOTTY;
  }
}

namespace FUSE
{
  int
  ioctl(const fuse_file_info_t *ffi_,
        unsigned long           cmd_,
        void                   *arg_,
        unsigned int            flags_,
        void                   *data_,
        uint32_t               *out_bufsz_)
  {
    if(l::is_mergerfs_ioctl_cmd(cmd_))
      return l::ioctl_custom(ffi_,cmd_,data_);

    if(flags_ & FUSE_IOCTL_DIR)
      return l::ioctl_dir(ffi_,cmd_,data_,out_bufsz_);

    return l::ioctl_file(ffi_,cmd_,data_,out_bufsz_);
  }
}
