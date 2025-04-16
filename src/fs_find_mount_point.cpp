#include "fs_find_mount_point.hpp"

#include "fs_lstat.hpp"

#include <errno.h>

static
inline
std::error_code
_ecerrno(void)
{
  return std::error_code(errno,std::generic_category());
}

fs::ecpath
fs::find_mount_point(const ghc::filesystem::path &path_)
{
  int rv;
  struct stat initial_st;
  struct stat tmp_st;
  std::error_code ec;
  ghc::filesystem::path can_path;
  ghc::filesystem::path tmp_path;

  can_path = ghc::filesystem::weakly_canonical(path_,ec);
  fmt::print("{}\n",can_path);
  if(ec)
    can_path = ghc::filesystem::absolute(path_,ec);
  if(ec)
    return nonstd::make_unexpected(ec);
  fmt::print("{}\n",can_path);  

  rv = fs::lstat(can_path,&initial_st);
  if(rv == -1)
    return nonstd::make_unexpected(::_ecerrno());

  tmp_path = can_path;
  while(tmp_path != tmp_path.root_path())
    {
      rv = fs::lstat(tmp_path.parent_path(),&tmp_st);
      if(rv == -1)
        return nonstd::make_unexpected(::_ecerrno());
      if(tmp_st.st_dev != initial_st.st_dev)
        return tmp_path;
      
      tmp_path = tmp_path.parent_path();
    }

  return tmp_path;
}
