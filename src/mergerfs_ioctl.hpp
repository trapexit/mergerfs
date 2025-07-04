#pragma once

#include <sys/ioctl.h>

#ifndef _IOC_TYPE
#define _IOC_TYPE(X) (((X) >> 8) & 0xFF)
#endif

#ifndef _IOC_SIZEBITS
#define _IOC_SIZEBITS 14
#endif

#define IOCTL_BUF_SIZE ((1 << _IOC_SIZEBITS) - 1)
typedef char IOCTL_BUF[IOCTL_BUF_SIZE];
#define IOCTL_APP_TYPE             0xDF
#define IOCTL_FILE_INFO            _IOWR(IOCTL_APP_TYPE,0,IOCTL_BUF)
#define IOCTL_GC                   _IO(IOCTL_APP_TYPE,1)
#define IOCTL_GC1                  _IO(IOCTL_APP_TYPE,2)
#define IOCTL_INVALIDATE_ALL_NODES _IO(IOCTL_APP_TYPE,3)
#define IOCTL_INVALIDATE_GID_CACHE _IO(IOCTL_APP_TYPE,4)
