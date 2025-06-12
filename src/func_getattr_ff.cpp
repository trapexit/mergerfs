#include "func_getattr_ff.hpp"

#include "fs_lstat.hpp"
#include "fs_inode.hpp"

std::string_view
Func2::GetAttrFF::name() const
{
  return "ff"sv;
}

int
Func2::GetAttrFF::operator()(const Branches  &branches_,
                             const fs::Path  &fusepath_,
                             struct stat     *st_,
                             fuse_timeouts_t *timeout_)
{
  int rv;
  fs::Path fullpath;

  for(const auto &branch : branches_)
    {
      fullpath = branch.path;
      fullpath += fusepath_;
      rv = fs::lstat(fullpath,st_);
      if(rv != 0)
        continue;

      fs::inode::calc(branch.path,fusepath_.string(),st_);
      return 0;
    }

  return -ENOENT;
}
