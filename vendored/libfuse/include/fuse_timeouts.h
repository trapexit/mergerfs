#pragma once

#include <stdint.h>

typedef struct fuse_timeouts_s fuse_timeouts_t;
struct fuse_timeouts_s
{
  uint64_t entry;
  uint64_t attr;
};
