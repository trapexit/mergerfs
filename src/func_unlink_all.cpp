#include "func_unlink_all.hpp"

#include "errno.hpp"
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

      const int rv = fs::unlink(fullpath);
      if(rv == -ENOENT)
        continue;

      found = true;
      err   = rv;
    }

  if(!found)
    return -ENOENT;

  return err;
}
