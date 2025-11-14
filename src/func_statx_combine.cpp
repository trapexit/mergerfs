#include "func_statx_combine.hpp"

#include "fs_inode.hpp"
#include "fs_statx.hpp"
#include "symlinkify.hpp"
#include "timespec_utils.hpp"


std::string_view
Func2::StatxCombine::name() const
{
  return "combine";
}

int
Func2::StatxCombine::operator()(const Branches           &branches_,
                                const fs::path           &fusepath_,
                                const u32                 flags_,
                                const u32                 mask_,
                                struct fuse_statx        *st_,
                                const FollowSymlinksEnum  follow_symlinks_,
                                const s64                 symlinkify_timeout_)
{
  int rv;
  fs::path fullpath;
  const Branch *first_branch;

  first_branch = nullptr;
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

      if(!first_branch)
        {
          *st_ = tmp_st;
          first_branch = &branch;
          continue;
        }

      st_->atime = TimeSpec::newest(st_->atime,tmp_st.atime);
      st_->ctime = TimeSpec::newest(st_->ctime,tmp_st.ctime);
      st_->mtime = TimeSpec::newest(st_->mtime,tmp_st.mtime);
      st_->btime = TimeSpec::newest(st_->btime,tmp_st.btime);
      st_->nlink += tmp_st.nlink;
    }

  if(!first_branch)
    return -ENOENT;

  if(symlinkify_timeout_ >= 0)
    {
      fullpath = first_branch->path / fusepath_;
      symlinkify::convert_if_can_be_symlink(fullpath,
                                            st_,
                                            symlinkify_timeout_);
    }

  fs::inode::calc(first_branch->path,fusepath_,st_);

  return 0;
}
