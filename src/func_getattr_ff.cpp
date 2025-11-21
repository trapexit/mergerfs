#include "func_getattr_ff.hpp"

#include "error.hpp"
#include "fs_stat.hpp"
#include "fs_inode.hpp"
#include "symlinkify.hpp"


std::string_view
Func2::GetAttrFF::name() const
{
  return "ff";
}

int
Func2::GetAttrFF::operator()(const Branches  &branches_,
                             const fs::path  &fusepath_,
                             struct stat     *st_,
                             const FollowSymlinksEnum follow_symlinks_,
                             const s64 symlinkify_timeout_)
{
  int rv;
  fs::path fullpath;
  Err err;

  for(const auto &branch : branches_)
    {
      fullpath = branch.path / fusepath_;
      rv = fs::stat(fullpath,st_,follow_symlinks_);
      if(rv < 0)
        {
          err = rv;
          continue;
        }

      symlinkify::convert_if_can_be_symlink(fullpath,
                                            st_,
                                            symlinkify_timeout_);

      fs::inode::calc(branch.path,fusepath_,st_);

      return rv;
    }



  return err;
}
