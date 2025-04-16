#include "fs_find_mount_point.hpp"

#include "fs_lstat.hpp"

ghc::filesystem::path
fs::find_mount_point(const ghc::filesystem::path &path_)
{
  int rv;
  struct stat initial_st;
  struct stat tmp_st;
  std::error_code ec;
  ghc::filesystem::path can_path;
  ghc::filesystem::path tmp_path;

  can_path = ghc::filesystem::weakly_canonical(path_,ec);
  
  rv = fs::lstat(can_path,&initial_st);
  if(rv == -1)
    return {};

  tmp_path = can_path;
  while(tmp_path != "/")
    {
      rv = fs::lstat(tmp_path.parent_path(),&tmp_st);
      if(rv == -1)
        return {};
      printf("%d:%s %d:%s\n",
             tmp_st.st_dev,
             tmp_path.parent_path().string().c_str(),
             initial_st.st_dev,
             can_path.string().c_str());
      if(tmp_st.st_dev != initial_st.st_dev)
        return tmp_path;
      
      tmp_path = tmp_path.parent_path();
    }

  return tmp_path;
}
