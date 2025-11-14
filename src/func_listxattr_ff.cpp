#include "func_listxattr_ff.hpp"

#include "error.hpp"
#include "fs_llistxattr.hpp"


std::string_view
Func2::ListxattrFF::name() const
{
  return "ff";
}

static
ssize_t
_listxattr_ff(const Branches &branches_,
              const fs::path &fusepath_,
              char           *list_,
              const size_t    size_)
{
  Err err;
  ssize_t rv;
  fs::path fullpath;

  for(const auto &branch : branches_)
    {
      fullpath = branch.path / fusepath_;

      rv = fs::llistxattr(fullpath,list_,size_);
      switch(rv)
        {
        case -ERANGE:
        case -ENOTSUP:
        case -E2BIG:
          return rv;
        default:
          if(rv >= 0)
            return rv;
          err = rv;
          break;
        }
    }

  return err;
}

ssize_t
Func2::ListxattrFF::operator()(const Branches &branches_,
                               const fs::path &fusepath_,
                               char           *list_,
                               const size_t    size_)
{
  return ::_listxattr_ff(branches_,fusepath_,list_,size_);
}
