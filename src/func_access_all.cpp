#include "func_access_all.hpp"

#include "error.hpp"
#include "fs_eaccess.hpp"


std::string_view
Func2::AccessAll::name() const
{
  return "all";
}

int
Func2::AccessAll::operator()(const Branches &branches_,
                             const fs::path &fusepath_,
                             const int       mode_)
{
  int rv;
  fs::path fullpath;

  for(const auto &branch : branches_)
    {
      fullpath = branch.path / fusepath_;

      rv = fs::eaccess(fullpath,mode_);
      if(rv < 0)
        return rv;
    }

  return 0;
}
