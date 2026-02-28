#pragma once

#include <stdint.h>

typedef struct fuse_entry_s fuse_entry_t;
struct fuse_entry_s
{
  uint64_t nodeid;
  uint64_t generation;
  uint64_t entry_valid;
  uint64_t attr_valid;
  uint32_t entry_valid_nsec;
  uint32_t attr_valid_nsec;
};
