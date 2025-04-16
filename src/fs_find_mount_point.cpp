#include "fs_find_mount_point.hpp"

#include "fs_lstat.hpp"

ghc::filesystem::path
fs::find_mount_point(const ghc::filesystem::path &path_)
{
  int rv;
  struct stat initial_st;
  struct stat tmp_st;
  ghc::filesystem::path tmp_path;

  rv = fs::lstat(path_,&initial_st);
  if(rv == -1)
    return {};

  while(tmp_path != "/")
    {
      rv = fs::lstat(tmp_path.parent_path(),&tmp_st);
      if(rv == -1)
        return {};
      printf("%d %d\n",
             tmp_st.st_dev,
             initial_st.st_dev);
      if(tmp_st.st_dev != initial_st.st_dev)
        return tmp_path;
      
      tmp_path = tmp_path.parent_path();
    }

  return {};
}
