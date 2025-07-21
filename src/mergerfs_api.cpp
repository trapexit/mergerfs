#include "mergerfs_api.hpp"

#include "fs_lgetxattr.hpp"
#include "str.hpp"

#include "scope_guard.hpp"

#include <array>
#include <cstring>


typedef std::array<char,64*1024> mfs_api_buf_t;

static
int
_lgetxattr(const std::string &input_path_,
           const std::string &key_,
           std::string       &value_)
{
  int rv;
  std::string key;
  mfs_api_buf_t buf;

  key = "user.mergerfs." + key_;
  rv = fs::lgetxattr(input_path_,key,buf.data(),buf.size());
  if(rv < 0)
    return rv;

  value_.clear();
  value_.reserve(rv);
  value_.append(buf.data(),(size_t)rv);

  return rv;
}

int
mergerfs::api::allpaths(const std::string        &input_path_,
                        std::vector<std::string> &output_paths_)
{
  int rv;
  std::string val;

  rv = ::_lgetxattr(input_path_,"allpaths",val);
  if(rv < 0)
    return rv;

  str::split_on_null(val.data(),val.size(),&output_paths_);

  return 0;
}

int
mergerfs::api::basepath(const std::string &input_path_,
                        std::string       &basepath_)
{
  return ::_lgetxattr(input_path_,"basepath",basepath_);
}

int
mergerfs::api::relpath(const std::string &input_path_,
                       std::string       &relpath_)
{
  return ::_lgetxattr(input_path_,"relpath",relpath_);
}

int
mergerfs::api::fullpath(const std::string &input_path_,
                        std::string       &fullpath_)
{
  return ::_lgetxattr(input_path_,"fullpath",fullpath_);
}
