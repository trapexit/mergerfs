#pragma once

#include "base_types.h"
#include "fs_path.hpp"

#include <sys/stat.h>

#define FS_COPYFILE_NONE (0)
#define FS_COPYFILE_CLEANUP_FAILURE (1 << 0)

namespace fs
{
  struct CopyFileFlags
  {
    int cleanup_failure:1;
  };

  s64
  copyfile(const int          src_fd,
           const struct stat &src_st,
           const int          dst_fd);

  s64
  copyfile(const int            src_fd,
           const fs::path      &dst_filepath,
           const CopyFileFlags &flags);

  s64
  copyfile(const fs::path      &src_filepath,
           const fs::path      &dst_filepath,
           const CopyFileFlags &flags);
}
