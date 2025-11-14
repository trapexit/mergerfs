#include "func_statx_ff.hpp"

#include "error.hpp"
#include "fs_statx.hpp"
#include "fs_inode.hpp"
#include "symlinkify.hpp"


std::string_view
Func2::StatxFF::name() const
{
  return "ff";
}

int
Func2::StatxFF::operator()(const Branches           &branches_,
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

  for(const auto &branch : branches_)
    {
      fullpath = branch.path / fusepath_;
      err = rv = fs::statx(AT_FDCWD,
                           fullpath,
                           flags_|AT_SYMLINK_NOFOLLOW,
                           mask_,
                           st_,
                           follow_symlinks_);
      if(rv < 0)
        continue;

      symlinkify::convert_if_can_be_symlink(fullpath,
                                            st_,
                                            symlinkify_timeout_);

      fs::inode::calc(branch.path,fusepath_,st_);

      return 0;
    }

  return err;
}
