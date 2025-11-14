#include "func_getxattr_ff.hpp"

#include "error.hpp"
#include "fs_lgetxattr.hpp"


std::string_view
Func2::GetxattrFF::name() const
{
  return "ff";
}

int
Func2::GetxattrFF::operator()(const Branches &branches_,
                              const fs::path &fusepath_,
                              const char     *attrname_,
                              char           *attrval_,
                              const size_t    attrvalsize_)
{
  int rv;
  Err err;
  fs::path fullpath;

  for(const auto &branch : branches_)
    {
      fullpath = branch.path / fusepath_;

      rv = fs::lgetxattr(fullpath,attrname_,attrval_,attrvalsize_);
      if(rv >= 0)
        return rv;
      err = rv;
    }

  return err;
}
