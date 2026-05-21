#include "func_removexattr_all.hpp"

#include "errno.hpp"
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

      const int rv = fs::lremovexattr(fullpath,attrname_);
      if(rv == -ENOENT)
        continue;

      found = true;
      err   = rv;
    }

  if(!found)
    return -ENOENT;

  return err;
}
