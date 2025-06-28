#pragma once

#include "int_types.h"

#include <filesystem>

#include <sys/stat.h>

#define FS_COPYFILE_NONE (0)
#define FS_COPYFILE_CLEANUP_FAILURE (1 << 0)


namespace fs
{
  int
  copyfile(const int          src_fd,
           const struct stat &src_st,
           const int          dst_fd);

  int
  copyfile(const std::filesystem::path &src,
           const std::filesystem::path &dst,
           const u32                    flags);
}
