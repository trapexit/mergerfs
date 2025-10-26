#pragma once

#include "int_types.h"

typedef struct fuse_conn_info_t fuse_conn_info_t;
struct fuse_conn_info_t
{
  u32 proto_major;
  u32 proto_minor;
  u64 capable;
  u64 want;
};
