#include "func_rmdir_all.hpp"

#include "config.hpp"
#include "error.hpp"
#include "fs_rmdir.hpp"
#include "fs_unlink.hpp"


std::string_view
Func2::RmdirAll::name() const
{
  return "all";
}

static
int
_should_unlink(int rv_)

{
  return ((rv_ == -ENOTDIR) &&
          (cfg.follow_symlinks != FollowSymlinks::ENUM::NEVER));
}

int
Func2::RmdirAll::operator()(const Branches &branches_,
                            const fs::path &fusepath_)
{
  int rv;
  Err err;
  fs::path fullpath;

  for(const auto &branch : branches_)
    {
      if(branch.ro())
        {
          err = -EROFS;
          continue;
        }

      fullpath = branch.path / fusepath_;

      rv = fs::rmdir(fullpath);
      if(::_should_unlink(rv))
        rv = fs::unlink(fullpath);
      err = rv;
    }

  return err;
}
