#include "func_ioctl_ff.hpp"

#include "endian.hpp"
#include "errno.hpp"
#include "fs_close.hpp"
#include "fs_ioctl.hpp"
#include "fs_open.hpp"

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

#ifndef O_NOATIME
#define O_NOATIME 0
#endif

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


std::string_view
Func2::IoctlFF::name() const
{
  return "ff";
}

int
Func2::IoctlFF::operator()(const Branches &branches_,
                           const fs::path &fusepath_,
                           const uint32_t  &cmd_,
                           void            *&data_,
                           uint32_t        *&out_bufsz_)
{
  fs::path fullpath;

  for(const auto &branch : branches_)
    {
      fullpath = branch.path / fusepath_;

      const int fd = fs::open(fullpath,O_RDONLY|O_NOATIME|O_NONBLOCK);
      if(fd == -ENOENT)
        continue;
      if(fd < 0)
        return fd;

      const int rv = ::_ioctl(fd,cmd_,data_,out_bufsz_);
      fs::close(fd);
      return rv;
    }

  return -ENOENT;
}
