#include "func_readlink_ff.hpp"

#include "error.hpp"
#include "fs_readlink.hpp"
#include "fs_lstat.hpp"
#include "symlinkify.hpp"

#include <cstring>


std::string_view
Func2::ReadlinkFF::name() const
{
  return "ff";
}

static
int
_readlink_symlinkify(const Branches &branches_,
                     const fs::path &fusepath_,
                     char           *buf_,
                     const size_t    bufsize_,
                     const time_t    symlinkify_timeout_)
{
  int rv;
  Err err;
  struct stat st;
  fs::path fullpath;

  for(const auto &branch : branches_)
    {
      fullpath = branch.path / fusepath_;

      err = rv = fs::lstat(fullpath,&st);
      if(rv < 0)
        continue;

      if(not symlinkify::can_be_symlink(st,symlinkify_timeout_))
        {
          err = rv = fs::readlink(fusepath_,buf_,bufsize_);
          if(rv < 0)
            continue;

          buf_[rv] = '\0';

          return 0;
        }

      strncpy(buf_,fullpath.c_str(),bufsize_);

      return 0;
    }

  return err;
}

static
int
_readlink(const Branches &branches_,
          const fs::path &fusepath_,
          char           *buf_,
          const size_t    bufsize_)
{
  int rv;
  Err err;
  fs::path fullpath;

  for(const auto &branch : branches_)
    {
      fullpath = branch.path / fusepath_;

      err = rv = fs::readlink(fusepath_,buf_,bufsize_);
      if(rv < 0)
        continue;

      buf_[rv] = '\0';

      return 0;
    }

  return err;
}



int
Func2::ReadlinkFF::operator()(const Branches &branches_,
                              const fs::path &fusepath_,
                              char           *buf_,
                              const size_t    bufsize_,
                              const time_t    symlinkify_timeout_)
{
  if(symlinkify_timeout_ > 0)
    return ::_readlink_symlinkify(branches_,
                                  fusepath_,
                                  buf_,
                                  bufsize_,
                                  symlinkify_timeout_);

  return ::_readlink(branches_,
                     fusepath_,
                     buf_,
                     bufsize_);
}
