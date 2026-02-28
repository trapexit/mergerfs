#pragma once

#include <stdint.h>

struct fuse_session;

typedef struct fuse_pollhandle_t fuse_pollhandle_t;
struct fuse_pollhandle_t
{
  uint64_t kh;
  struct fuse_session *se;
};
