#include "fs_find_mount_point.hpp"

#include "fs_lstat.hpp"

static
inline
std::error_code
ecerrno(const int errno_ = errno)
{
  return std::make_error_code(errno_,std::generic_category());
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
  if(ec)
    can_path = ghc::filesystem::absolute(path_,ec);
  if(ec)
    return nonstd::make_unexpected(ec);

  rv = fs::lstat(can_path,&initial_st);
  if(rv == -1)
    return nonstd::make_unexpected(::_ecerrno())
    {
      ec.assign(errno,std::generic_category());      
      return nonstd::make_unexpected(std::error_code{errno,std::generic_category()});
    }

  tmp_path = can_path;
  while(tmp_path != tmp_path.root_path())
    {
      rv = fs::lstat(tmp_path.parent_path(),&tmp_st);
      if(rv == -1)
        {
          ec.assign(errno,std::generic_category());
          return nonstd::make_unexpected(ec);
        }
      if(tmp_st.st_dev != initial_st.st_dev)
        return tmp_path;
      
      tmp_path = tmp_path.parent_path();
    }

  return tmp_path;
}
