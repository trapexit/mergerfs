#include "func_readlink_ff.hpp"

#include "errno.hpp"
#include "fs_lstat.hpp"
#include "fs_readlink.hpp"
#include "symlinkify.hpp"

#include <algorithm>
#include <cstring>


std::string_view
Func2::ReadlinkFF::name() const
{
  return "ff";
}

int
Func2::ReadlinkFF::operator()(const Branches &branches_,
                              const fs::path &fusepath_,
                              char           *buf_,
                              const size_t    bufsize_,
                              const bool      symlinkify_,
                              const time_t    symlinkify_timeout_)
{
  fs::path fullpath;

  if(!symlinkify_)
    {
      for(const auto &branch : branches_)
        {
          fullpath = branch.path / fusepath_;

          const int rv = fs::readlink(fullpath,buf_,bufsize_);
          if(rv == -ENOENT)
            continue;

          return rv;
        }

      return -ENOENT;
    }

  for(const auto &branch : branches_)
    {
      struct stat st;

      fullpath = branch.path / fusepath_;

      const int rv = fs::lstat(fullpath,&st);
      if(rv == -ENOENT)
        continue;
      if(rv < 0)
        return rv;

      if(!symlinkify::can_be_symlink(st,symlinkify_timeout_))
        return fs::readlink(fullpath,buf_,bufsize_);

      const std::string fullpath_str = fullpath.string();
      const size_t len = std::min(fullpath_str.size(),bufsize_);
      memcpy(buf_,fullpath_str.c_str(),len);

      return len;
    }

  return -ENOENT;
}
