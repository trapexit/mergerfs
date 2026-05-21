#include "func_listxattr_ff.hpp"

#include "errno.hpp"
#include "fs_llistxattr.hpp"


std::string_view
Func2::ListxattrFF::name() const
{
  return "ff";
}

ssize_t
Func2::ListxattrFF::operator()(const Branches &branches_,
                               const fs::path &fusepath_,
                               char           *list_,
                               const size_t    size_)
{
  fs::path fullpath;

  for(const auto &branch : branches_)
    {
      fullpath = branch.path / fusepath_;

      const ssize_t rv = fs::llistxattr(fullpath,list_,size_);
      if(rv == -ENOENT)
        continue;

      return rv;
    }

  return -ENOENT;
}
