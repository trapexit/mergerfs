#include "fs_find_mount_point.hpp"

#include "fs_lstat.hpp"

int
fs::find_mount_point(const ghc::filesystem::path &path_)
{
  int rv;
  struct stat initial_st;
  struct stat lower_st;

  rv = fs::lstat(path_,&initial_st);
  if(rv == -1)
    return -1;

  

  return 0;
}
