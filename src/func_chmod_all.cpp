#include "func_chmod_all.hpp"

#include "errno.hpp"
#include "fs_lchmod.hpp"


std::string_view
Func2::ChmodAll::name() const
{
  return "all";
}

int
Func2::ChmodAll::operator()(const Branches &branches_,
                            const fs::path &fusepath_,
                            const mode_t    mode_)
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

      const int rv = fs::lchmod(fullpath,mode_);
      if(rv == -ENOENT)
        continue;

      if(!found)
        { err = rv; found = true; continue; }
      if(rv == 0)
        { err = 0; continue; }
      if(err == 0)
        continue;
      err = rv;
    }

  if(!found)
    return -ENOENT;

  return err;
}
