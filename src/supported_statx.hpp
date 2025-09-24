#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <sys/stat.h>

#ifdef STATX_TYPE
#define MERGERFS_SUPPORTED_STATX
#endif
