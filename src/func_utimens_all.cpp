#include "func_utimens_all.hpp"

#include "error.hpp"
#include "fs_lutimens.hpp"


std::string_view
Func2::UtimensAll::name() const
{
  return "all";
}

int
Func2::UtimensAll::operator()(const Branches &branches_,
                              const fs::path &fusepath_,
                              const timespec times_[2])
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

      err = fs::lutimens(fullpath,times_);
    }

  return err;
}
