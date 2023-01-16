#pragma once

#include <stdint.h>

typedef struct fuse_msgbuf_t fuse_msgbuf_t;
struct fuse_msgbuf_t
{
  char     *mem;
  uint32_t  size;
  uint32_t  used;
  int       pipefd[2];
};
