#include "func_statx_newest.hpp"

#include "fs_statx.hpp"
#include "fs_inode.hpp"
#include "symlinkify.hpp"
#include "timespec_utils.hpp"


std::string_view
Func2::StatxNewest::name() const
{
  return "newest";
}

int
Func2::StatxNewest::operator()(const Branches           &branches_,
                               const fs::path           &fusepath_,
                               const u32                 flags_,
                               const u32                 mask_,
                               struct fuse_statx        *st_,
                               const FollowSymlinksEnum  follow_symlinks_,
                               const s64                 symlinkify_timeout_)
{
  int rv;
  fs::path fullpath;
  const Branch *newest_branch;

  newest_branch = nullptr;
  for(const auto &branch : branches_)
    {
      struct fuse_statx tmp_st;

      fullpath = branch.path / fusepath_;
      rv = fs::statx(AT_FDCWD,
                     fullpath,
                     flags_|AT_SYMLINK_NOFOLLOW,
                     mask_,
                     &tmp_st,
                     follow_symlinks_);
      if(rv < 0)
        continue;

      if(not TimeSpec::is_newer(tmp_st.mtime,st_->mtime))
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
