#pragma once

#include "int_types.h"

#include <climits>
#include <string>

#define FUSE_CFG_INVALID_ID    -1
#define FUSE_CFG_INVALID_UMASK -1


struct fuse_cfg_t
{
  s64 uid = FUSE_CFG_INVALID_ID;
  bool valid_uid() const;

  s64 gid = FUSE_CFG_INVALID_ID;
  bool valid_gid() const;

  s64 umask = FUSE_CFG_INVALID_UMASK;
  bool valid_umask() const;

  s64 remember_nodes = 0;

  bool debug = false;

  int max_background = 0;
  int congestion_threshold = 0;
  u32 max_pages = 32;
  int passthrough_max_stack_depth = 1;

  int read_thread_count = 0;
  int process_thread_count = -1;
  int process_thread_queue_depth = 2;
  std::string pin_threads = "false";
};

extern fuse_cfg_t fuse_cfg;
