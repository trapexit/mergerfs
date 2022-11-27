#pragma once

#include <stdint.h>

struct fuse_chan;
struct fuse_ll;

typedef struct fuse_pollhandle_t fuse_pollhandle_t;
struct fuse_pollhandle_t
{
  uint64_t kh;
  struct fuse_chan *ch;
  struct fuse_ll *f;
};
