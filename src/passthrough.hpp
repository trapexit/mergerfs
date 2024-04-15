#pragma once

#include <mutex>
#include <string>

struct PTInfo
{
  PTInfo()
    : path_fd(-1),
      backing_id(0),
      open_count(0)
  {
  }

  PTInfo(PTInfo &&pti_)
  {
    std::lock_guard<std::mutex> _(mutex);

    path_fd    = pti_.path_fd;
    backing_id = pti_.backing_id;
    filepath   = pti_.filepath;
    open_count = pti_.open_count;
  }

  int path_fd;
  int backing_id;
  unsigned open_count;
  std::string filepath;
  mutable std::mutex mutex;
};

namespace PassthroughStuff
{
  PTInfo* get_pti(const char *fusepath);
}
