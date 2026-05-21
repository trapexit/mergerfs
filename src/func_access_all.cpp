#include "func_access_all.hpp"
#include "errno.hpp"
#include "fs_eaccess.hpp"

std::string_view Func2::AccessAll::name() const { return "all"; }

// "all": every branch must satisfy eaccess (including the file existing
// there). Returns the first failure.
int
Func2::AccessAll::operator()(const Branches &branches_,
                             const fs::path &fusepath_,
                             const int       mode_)
{
  bool any = false;
  for(const auto &branch : branches_)
    {
      any = true;
      const fs::path fullpath = branch.path / fusepath_;
      const int rv = fs::eaccess(fullpath,mode_);
      if(rv < 0)
        return rv;
    }
  if(!any)
    return -ENOENT;
  return 0;
}
