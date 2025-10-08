#pragma once

#include <stdint.h>

typedef struct fuse_req_ctx_t fuse_req_ctx_t;
struct fuse_req_ctx_t
{
  uint32_t len;
  uint32_t opcode;
  uint64_t unique;
  uint64_t nodeid;
  uint32_t uid;
  uint32_t gid;
  uint32_t pid;
  uint32_t umask;
};
