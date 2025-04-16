#include "fs_find_mount_point.hpp"

#include "fs_lstat.hpp"

int
fs::find_mount_point(const ghc::filesystem::path &path_)
{
  int rv;
  struct stat initial_st;

  rv = fs::lstat(path_,&initial_st);

  return 0;
}
