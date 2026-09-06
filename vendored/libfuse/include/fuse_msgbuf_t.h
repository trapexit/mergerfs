#pragma once

#include <stdint.h>

/* No separate "typedef struct fuse_msgbuf_t fuse_msgbuf_t;" - that's a
   C idiom (a tag name isn't usable as a type name on its own in C);
   in C++ the struct's tag is already a type name. */
struct fuse_msgbuf_t
{
  uint32_t  size;
  char     *mem;
  int       pipefd[2] = {-1,-1};
  uint32_t  pipe_used = 0;
  uint32_t  pipe_cap  = 0;
};
