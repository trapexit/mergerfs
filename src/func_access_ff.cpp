#include "func_access_ff.hpp"

#include "errno.hpp"
#include "fs_eaccess.hpp"


std::string_view
Func2::AccessFF::name() const
{
  return "ff";
}

int
Func2::AccessFF::operator()(const Branches &branches_,
                            const fs::path &fusepath_,
                            const int       mode_)
{
  fs::path fullpath;

  for(const auto &branch : branches_)
    {
      fullpath = branch.path / fusepath_;

      const int rv = fs::eaccess(fullpath,mode_);
      if(rv == -ENOENT)
        continue;

      return rv;
    }

  return -ENOENT;
}
