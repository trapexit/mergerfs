#include "func_setxattr_all.hpp"

#include "error.hpp"
#include "fs_lsetxattr.hpp"


std::string_view
Func2::SetxattrAll::name() const
{
  return "all";
}

int
Func2::SetxattrAll::operator()(const Branches &branches_,
                               const fs::path &fusepath_,
                               const char     *attrname_,
                               const char     *attrval_,
                               size_t          attrvalsize_,
                               int             flags_)
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

      err = fs::lsetxattr(fullpath,attrname_,attrval_,attrvalsize_,flags_);
    }

  return err;
}
