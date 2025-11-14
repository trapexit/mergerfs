#include "func_truncate_all.hpp"

#include "error.hpp"
#include "fs_truncate.hpp"


std::string_view
Func2::TruncateAll::name() const
{
  return "all";
}

int
Func2::TruncateAll::operator()(const Branches &branches_,
                               const fs::path &fusepath_,
                               const off_t     size_)
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

      err = fs::truncate(fullpath,size_);
    }

  return err;
}
