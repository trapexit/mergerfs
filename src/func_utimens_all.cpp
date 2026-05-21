#include "func_utimens_all.hpp"

#include "errno.hpp"
#include "fs_lutimens.hpp"


std::string_view
Func2::UtimensAll::name() const
{
  return "all";
}

int
Func2::UtimensAll::operator()(const Branches &branches_,
                              const fs::path &fusepath_,
                              const timespec  times_[2])
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

      const int rv = fs::lutimens(fullpath,times_);
      if(rv == -ENOENT)
        continue;

      found = true;
      err   = rv;
    }

  if(!found)
    return -ENOENT;

  return err;
}
