#include "func_getattr_combine.hpp"

#include "fs_lstat.hpp"
#include "fs_inode.hpp"
#include "timespec_utils.hpp"

int
Func2::GetAttrCombine::operator()(const Branches  &branches_,
                                  const fs::Path  &fusepath_,
                                  struct stat     *st_,
                                  fuse_timeouts_t *timeout_)
{
  int rv;
  fs::Path fullpath;

  for(const auto &branch : branches_)
    {
      struct stat st;

      fullpath = branch.path;
      fullpath += fusepath_;
      rv = fs::lstat(fullpath.c_str(),&st);
      if(rv == -1)
        continue;

      if(st_->st_ino == 0)
        {
          *st_ = st;
          fs::inode::calc(branch.path,fusepath_.string(),st_);
          continue;
        }

      st_->st_atim = TimeSpec::newest(st_->st_atim,st.st_atim);
      st_->st_ctim = TimeSpec::newest(st_->st_ctim,st.st_ctim);
      st_->st_mtim = TimeSpec::newest(st_->st_mtim,st.st_mtim);
      st_->st_nlink += st.st_nlink;
    }

  if(st_->st_ino == 0)
    return -ENOENT;

  return 0;
}
