#include "func_getattr_newest.hpp"

#include "fs_stat.hpp"
#include "fs_inode.hpp"
#include "timespec_utils.hpp"
#include "symlinkify.hpp"


std::string_view
Func2::GetAttrNewest::name() const
{
  return "newest";
}

int
Func2::GetAttrNewest::operator()(const Branches  &branches_,
                                 const fs::path  &fusepath_,
                                 struct stat     *st_,
                                 const FollowSymlinksEnum follow_symlinks_,
                                 const s64 symlinkify_timeout_)
{
  int rv;
  fs::path fullpath;
  const Branch *newest_branch;

  newest_branch = nullptr;
  for(const auto &branch : branches_)
    {
      struct stat tmp_st;

      fullpath = branch.path / fusepath_;
      rv = fs::stat(fullpath,&tmp_st,follow_symlinks_);
      if(rv < 0)
        continue;

      if(!TimeSpec::is_newer(tmp_st.st_mtim,st_->st_mtim))
        continue;

      *st_ = tmp_st;
      newest_branch = &branch;
    }

  if(!newest_branch)
    return -ENOENT;

  if(symlinkify_timeout_ >= 0)
    {
      fullpath = newest_branch->path / fusepath_;
      symlinkify::convert_if_can_be_symlink(fullpath,
                                            st_,
                                            symlinkify_timeout_);
    }

  fs::inode::calc(newest_branch->path,fusepath_,st_);

  return 0;
}
