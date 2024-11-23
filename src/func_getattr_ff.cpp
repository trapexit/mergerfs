#include "func_getattr_ff.hpp"

#include "fs_lstat.hpp"
#include "fs_inode.hpp"

int
Func2::GetattrFF::process(const Branches  &branches_,
                          const fs::Path  &fusepath_,
                          struct stat     *st_,
                          fuse_timeouts_t *timeout_)
{
  int rv;
  Branches::CPtr branches;
  fs::Path fullpath;

  branches = branches_;
  
  for(const auto &branch : *branches)
    {
      fullpath = branch.path;
      fullpath += fusepath_;
      rv = fs::lstat(fullpath.c_str(),st_);
      if(rv == 0)
        {
          fs::inode::calc(fusepath_,st_);
          return 0;
        }
    }

  return -ENOENT;
}
