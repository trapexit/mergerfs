#pragma once

#include <stdint.h>

typedef struct fuse_attr_s fuse_attr_t;
struct fuse_attr_s
{
  uint64_t ino;
  uint64_t size;
  uint64_t blocks;
  uint64_t atime;
  uint64_t mtime;
  uint64_t ctime;
  uint32_t atimensec;
  uint32_t mtimensec;
  uint32_t ctimensec;
  uint32_t mode;
  uint32_t nlink;
  uint32_t uid;
  uint32_t gid;
  uint32_t rdev;
  uint32_t blksize;
  uint32_t _padding;
};
