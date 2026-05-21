#include "func_truncate_all.hpp"

#include "errno.hpp"
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

      const int rv = fs::truncate(fullpath,size_);
      if(rv == -ENOENT)
        continue;

      found = true;
      err   = rv;
    }

  if(!found)
    return -ENOENT;

  return err;
}
