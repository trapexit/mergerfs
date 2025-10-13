#pragma once

#include "fuse_req_ctx.h"

struct fuse_ll;
struct fuse_chan;

typedef struct fuse_req_t fuse_req_t;
struct fuse_req_t
{
  fuse_req_ctx_t ctx;
  struct fuse_ll *f;
  struct fuse_chan *ch;
  unsigned int ioctl_64bit : 1;
};
