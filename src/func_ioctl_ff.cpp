#include "func_ioctl_ff.hpp"

#include "error.hpp"
#include "fs_ioctl.hpp"


std::string_view
Func2::IoctlFF::name() const
{
  return "ff";
}

int
Func2::IoctlFF::operator()(const Branches &branches_,
                            const fs::path &fusepath_)
{
  Err err;
  fs::path fullpath;

  for(const auto &branch : branches_)
    {
    }

  return err;
}
