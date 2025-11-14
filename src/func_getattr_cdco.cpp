#include "func_getattr_cdco.hpp"

#include "error.hpp"
#include "fs_inode.hpp"
#include "fs_stat.hpp"
#include "symlinkify.hpp"
#include "timespec_utils.hpp"


std::string_view
Func2::GetAttrCDCO::name() const
{
  return "cdco";
}

int
Func2::GetAttrCDCO::operator()(const Branches &branches_,
                               const fs::path &fusepath_,
                               struct stat    *st_,
                               const FollowSymlinksEnum follow_symlinks_,
                               const s64 symlinkify_timeout_)
{
  int rv;
  Err err;
  fs::path fullpath;
  const Branch *first_branch;

  first_branch = nullptr;
  for(const auto &branch : branches_)
    {
      struct stat tmp_st;

      fullpath = branch.path / fusepath_;

      err = rv = fs::stat(fullpath,&tmp_st,follow_symlinks_);
      if(rv < 0)
        continue;

      if(not first_branch)
        {
          *st_ = tmp_st;
          first_branch = &branch;
          continue;
        }

      // This upgrades the uid:gid because mergerfs now does most
      // file interaction as root and relies on `default_permissions`
      // to manage permissions. Want to ensure that root owned files
      // can't be changed so we treat them all as root owned.
      if(tmp_st.st_uid == 0)
        st_->st_uid = 0;
      if(tmp_st.st_gid == 0)
        st_->st_gid = 0;
      st_->st_atim = TimeSpec::newest(st_->st_atim,tmp_st.st_atim);
      st_->st_ctim = TimeSpec::newest(st_->st_ctim,tmp_st.st_ctim);
      st_->st_mtim = TimeSpec::newest(st_->st_mtim,tmp_st.st_mtim);
      st_->st_nlink  += tmp_st.st_nlink;
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
