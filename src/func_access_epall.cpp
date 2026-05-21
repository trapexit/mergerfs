#include "func_access_epall.hpp"
#include "errno.hpp"
#include "fs_eaccess.hpp"

std::string_view Func2::AccessEPAll::name() const { return "epall"; }

int
Func2::AccessEPAll::operator()(const Branches &branches_,
                               const fs::path &fusepath_,
                               const int       mode_)
{
  bool found = false;
  for(const auto &branch : branches_)
    {
      const fs::path fullpath = branch.path / fusepath_;
      const int rv = fs::eaccess(fullpath,mode_);
      if(rv == -ENOENT)
        continue;
      found = true;
      if(rv < 0)
        return rv;
    }
  if(!found)
    return -ENOENT;
  return 0;
}
