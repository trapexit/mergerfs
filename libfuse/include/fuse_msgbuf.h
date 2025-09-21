#pragma once

#include <stdint.h>

typedef struct fuse_msgbuf_t fuse_msgbuf_t;
struct fuse_msgbuf_t
{
  uint32_t  size;
  char     *mem;
};
