#include "func_removexattr_all.hpp"

#include "error.hpp"
#include "fs_lremovexattr.hpp"


std::string_view
Func2::RemovexattrAll::name() const
{
  return "all";
}

int
Func2::RemovexattrAll::operator()(const Branches &branches_,
                                  const fs::path &fusepath_,
                                  const char     *attrname_)
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

      err = fs::lremovexattr(fullpath,attrname_);
    }

  return err;
}
