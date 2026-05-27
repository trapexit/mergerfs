#include "func_getattr_newest.hpp"

#include "fs_inode.hpp"
#include "fs_stat.hpp"
#include "symlinkify.hpp"
#include "timespec_utils.hpp"


std::string_view
Func2::GetAttrNewest::name() const
{

  return "newest";
}

int
Func2::GetAttrNewest::operator()(const Branches           &branches_,
                                 const fs::path           &fusepath_,
                                 struct stat              *st_,
                                 const FollowSymlinksEnum  follow_symlinks_,
                                 const bool                symlinkify_,
                                 const s64                 symlinkify_timeout_)
{
  int rv;
  int err;
  fs::path fullpath;
  const Branch *newest_branch;

  err = 0;
  newest_branch = nullptr;
  Branches::Ptr branches = branches_;
  for(const auto &branch : *branches)
    {
      struct stat tmp_st;

      fullpath = branch.path / fusepath_;
      rv = fs::stat(fullpath,&tmp_st,follow_symlinks_);
      if(rv < 0)
        {
          err = rv;
          continue;
        }

      if(newest_branch && (st_->st_mtim > tmp_st.st_mtim))
        continue;

      *st_ = tmp_st;
      newest_branch = &branch;
    }

  if(!newest_branch)
    return err;

  if(symlinkify_)
    {
      fullpath = newest_branch->path / fusepath_;
      symlinkify::convert_if_can_be_symlink(fullpath,
                                            st_,
                                            symlinkify_timeout_);
    }

  fs::inode::calc(newest_branch->path,fusepath_,st_);

  return 0;
}
