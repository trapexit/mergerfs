#include "func_unlink_all.hpp"

#include "error.hpp"
#include "fs_unlink.hpp"


std::string_view
Func2::UnlinkAll::name() const
{
  return "all";
}

int
Func2::UnlinkAll::operator()(const Branches &branches_,
                             const fs::path &fusepath_)
{
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

      err = fs::unlink(fullpath);
    }

  return err;
}
