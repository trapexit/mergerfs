#pragma once

#include "fuse_req_ctx.h"
#include "fuse_conn_info.hpp"

struct fuse_chan;

typedef struct fuse_req_t fuse_req_t;
struct fuse_req_t
{
  fuse_req_ctx_t ctx;
  struct fuse_chan *ch;
  unsigned int ioctl_64bit : 1;
  fuse_conn_info_t conn;
};

fuse_req_t* fuse_req_alloc();
void fuse_req_free(fuse_req_t *);
