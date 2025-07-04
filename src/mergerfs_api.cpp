#include "mergerfs_api.hpp"

#include "fs_close.hpp"
#include "fs_ioctl.hpp"
#include "fs_open.hpp"
#include "mergerfs_ioctl.hpp"
#include "fs_lgetxattr.hpp"

#include "str.hpp"

#include "scope_guard.hpp"

#include <string.h>


int
mergerfs::api::allpaths(const std::string        &input_path_,
                        std::vector<std::string> &output_paths_)
{
  int rv;
  IOCTL_BUF buf;

  rv = fs::lgetxattr(input_path_,"user.mergerfs.allpaths",buf,sizeof(buf));
  if(rv < 0)
    return rv;

  str::split_on_null(buf,rv,&output_paths_);

  return 0;
}

int
_lgetxattr(const std::string &input_path_,
           const std::string &key_,
           std::string       &value_)
{
  int rv;
  IOCTL_BUF buf;

  rv = fs::lgetxattr(input_path_,key_,buf,sizeof(buf));
  if(rv < 0)
    return rv;

  value_ = buf;

  return rv;
}

int
mergerfs::api::basepath(const std::string &input_path_,
                        std::string       &basepath_)
{
  return ::_lgetxattr(input_path_,"user.mergerfs.basepath",basepath_);
}

int
mergerfs::api::relpath(const std::string &input_path_,
                       std::string       &relpath_)
{
  return ::_lgetxattr(input_path_,"user.mergerfs.relpath",relpath_);
}

int
mergerfs::api::fullpath(const std::string &input_path_,
                        std::string       &fullpath_)
{
  return ::_lgetxattr(input_path_,"user.mergerfs.fullpath",fullpath_);
}
