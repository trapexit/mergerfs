#include "func_rmdir_all.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fs_rmdir.hpp"
#include "fs_unlink.hpp"


std::string_view
Func2::RmdirAll::name() const
{
  return "all";
}

static
bool
_should_unlink(int rv_)
{
  return ((rv_ == -ENOTDIR) &&
          (cfg.follow_symlinks != FollowSymlinks::ENUM::NEVER));
}

int
Func2::RmdirAll::operator()(const Branches &branches_,
                            const fs::path &fusepath_)
{
  int err;
  bool found;
  fs::path fullpath;

  err   = 0;
  found = false;
  for(const auto &branch : branches_)
    {
      if(branch.ro())
        continue;
      fullpath = branch.path / fusepath_;

      int rv = fs::rmdir(fullpath);
      if(::_should_unlink(rv))
        rv = fs::unlink(fullpath);
      if(rv == -ENOENT)
        continue;

      found = true;
      err   = rv;
    }

  if(!found)
    return -ENOENT;

  return err;
}
