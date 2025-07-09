#pragma once

#include "int_types.h"

#include <sys/ioctl.h>

#ifndef _IOC_TYPE
#define _IOC_TYPE(X) (((X) >> 8) & 0xFF)
#endif

#ifndef _IOC_SIZEBITS
#define _IOC_SIZEBITS 14
#endif

#define MERGERFS_IOCTL_BUF_SIZE ((1 << _IOC_SIZEBITS) - 1)

#pragma pack(push,1)
struct mergerfs_ioctl_t
{
  u32 size;
  char buf[MERGERFS_IOCTL_BUF_SIZE - sizeof(u32)];
};
#pragma pack(pop)

#define MERGERFS_IOCTL_APP_TYPE 0xDF
#define MERGERFS_IOCTL_GET      _IOWR(MERGERFS_IOCTL_APP_TYPE,0,mergerfs_ioctl_t)
#define MERGERFS_IOCTL_SET      _IOWR(MERGERFS_IOCTL_APP_TYPE,1,mergerfs_ioctl_t)
