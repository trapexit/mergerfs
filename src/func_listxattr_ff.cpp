#include "func_listxattr_ff.hpp"

#include "error.hpp"
#include "fs_llistxattr.hpp"


std::string_view
Func2::ListxattrFF::name() const
{
  return "ff";
}

int
Func2::ListxattrFF::operator()(const Branches &branches_,
                               const fs::path &fusepath_,
                               char           *list_,
                               const size_t    size_)
{
  Err err;
  fs::path fullpath;

  for(const auto &branch : branches_)
    {
    }

  return err;
}
