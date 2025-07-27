#include "func_getattr_combine.hpp"

#include "fs_inode.hpp"
#include "fs_stat.hpp"
#include "symlinkify.hpp"
#include "timespec_utils.hpp"


std::string_view
Func2::GetAttrCombine::name() const
{
  return "combine";
}

int
Func2::GetAttrCombine::operator()(const Branches &branches_,
                                  const fs::Path &fusepath_,
                                  struct stat    *st_,
                                  const FollowSymlinksEnum follow_symlinks_,
                                  const s64 symlinkify_timeout_)
{
  int rv;
  fs::Path fullpath;
  const Branch *first_branch;

  first_branch = nullptr;
  for(const auto &branch : branches_)
    {
      struct stat st;

      fullpath = branch.path;
      fullpath += fusepath_;
      rv = fs::stat(fullpath.c_str(),&st,follow_symlinks_);
      if(rv == -1)
        continue;

      if(!first_branch)
        {
          *st_ = st;
          first_branch = &branch;
          continue;
        }

      st_->st_atim = TimeSpec::newest(st_->st_atim,st.st_atim);
      st_->st_ctim = TimeSpec::newest(st_->st_ctim,st.st_ctim);
      st_->st_mtim = TimeSpec::newest(st_->st_mtim,st.st_mtim);
      st_->st_nlink += st.st_nlink;
    }

  if(!first_branch)
    return -ENOENT;

  if(symlinkify_timeout_ >= 0)
    {
      fullpath = fs::path::make(first_branch->path,fusepath_);
      symlinkify::convert_if_can_be_symlink(fullpath,
                                            st_,
                                            symlinkify_timeout_);
    }

  fs::inode::calc(first_branch->path,fusepath_.string(),st_);

  return 0;
}
