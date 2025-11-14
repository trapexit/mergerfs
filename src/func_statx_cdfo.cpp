#include "func_statx_cdfo.hpp"

#include "error.hpp"
#include "fs_inode.hpp"
#include "fs_statx.hpp"
#include "symlinkify.hpp"
#include "timespec_utils.hpp"


std::string_view
Func2::StatxCDFO::name() const
{
  return "cdfo";
}

int
Func2::StatxCDFO::operator()(const Branches           &branches_,
                             const fs::path           &fusepath_,
                             const u32                 flags_,
                             const u32                 mask_,
                             struct fuse_statx        *st_,
                             const FollowSymlinksEnum  follow_symlinks_,
                             const s64                 symlinkify_timeout_)
{
  int rv;
  Err err;
  fs::path fullpath;
  const Branch *first_branch;

  first_branch = nullptr;
  for(const auto &branch : branches_)
    {
      struct fuse_statx tmp_st;

      fullpath = branch.path / fusepath_;

      err = rv = fs::statx(AT_FDCWD,
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
          if(not S_ISDIR(tmp_st.mode))
            break;
          continue;
        }

      // This upgrades the uid:gid because mergerfs now does most
      // file interaction as root and relies on `default_permissions`
      // to manage permissions. Want to ensure that root owned files
      // can't be changed so we treat them all as root owned.
      if(tmp_st.uid == 0)
        st_->uid = 0;
      if(tmp_st.gid == 0)
        st_->gid = 0;
      st_->atime = TimeSpec::newest(st_->atime,tmp_st.atime);
      st_->ctime = TimeSpec::newest(st_->ctime,tmp_st.ctime);
      st_->mtime = TimeSpec::newest(st_->mtime,tmp_st.mtime);
      st_->btime = TimeSpec::newest(st_->btime,tmp_st.btime);
      st_->nlink += tmp_st.nlink;
    }

  if(!first_branch)
    return err;

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
