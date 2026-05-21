#include "func_setxattr_all.hpp"

#include "errno.hpp"
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

      const int rv = fs::lsetxattr(fullpath,attrname_,attrval_,attrvalsize_,flags_);
      if(rv == -ENOENT)
        continue;

      found = true;
      err   = rv;
    }

  if(!found)
    return -ENOENT;

  return err;
}
