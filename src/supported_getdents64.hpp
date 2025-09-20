#pragma once

#ifdef __linux__

#include <sys/syscall.h>

#ifdef SYS_getdents64
#define MERGERFS_SUPPORTED_GETDENTS64
#endif

#endif
