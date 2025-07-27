#pragma once

#include "int_types.h"

#include <filesystem>

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
  copyfile(const int                    src_fd,
           const std::filesystem::path &dst_filepath,
           const CopyFileFlags         &flags);

  s64
  copyfile(const std::filesystem::path &src_filepath,
           const std::filesystem::path &dst_filepath,
           const CopyFileFlags         &flags);
}
