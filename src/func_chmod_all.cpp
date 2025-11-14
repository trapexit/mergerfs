#include "func_chmod_all.hpp"

#include "error.hpp"
#include "fs_lchmod.hpp"


std::string_view
Func2::ChmodAll::name() const
{
  return "all";
}

int
Func2::ChmodAll::operator()(const Branches &branches_,
                            const fs::path &fusepath_,
                            const mode_t    mode_)
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

      err = fs::lchmod(fullpath,mode_);
    }

  return err;
}
