#include "func_chown_all.hpp"

#include "error.hpp"
#include "fs_lchown.hpp"


std::string_view
Func2::ChownAll::name() const
{
  return "all";
}

int
Func2::ChownAll::operator()(const Branches &branches_,
                            const fs::path &fusepath_,
                            const uid_t     uid_,
                            const gid_t     gid_)
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

      err = fs::lchown(fullpath,uid_,gid_);
    }

  return err;
}
