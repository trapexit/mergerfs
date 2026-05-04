#pragma once

#include "base_types.h"

#include <atomic>
#include <climits>
#include <cstdio>
#include <memory>
#include <string>

#define FUSE_CFG_INVALID_ID    -1
#define FUSE_CFG_INVALID_UMASK -1


struct fuse_cfg_t
{
  template<typename T>
  using sp = std::shared_ptr<T>;

  s64 uid = FUSE_CFG_INVALID_ID;
  bool valid_uid() const;

  s64 gid = FUSE_CFG_INVALID_ID;
  bool valid_gid() const;

  s64 umask = FUSE_CFG_INVALID_UMASK;
  bool valid_umask() const;

  std::atomic<s64> remember_nodes = 0;

  std::atomic<bool> debug = false;
  std::shared_ptr<FILE> log_file() const;
  void log_file(std::shared_ptr<FILE>);
  std::shared_ptr<std::string> log_filepath() const;
  void log_filepath(const std::string &);
private:
  std::shared_ptr<FILE>        _log_file     = sp<FILE>(stderr,[](FILE*){});
  std::shared_ptr<std::string> _log_filepath = sp<std::string>();

public:
  int max_background = 0;
  int congestion_threshold = 0;
  u32 max_pages = 32;
  int passthrough_max_stack_depth = 1;

  int read_thread_count = 0;
  int process_thread_count = -1;
  int process_thread_queue_depth = 2;
  std::string pin_threads = "false";

  u16 request_timeout = 0;
};

extern fuse_cfg_t fuse_cfg;
