#include "mergerfs_api.hpp"

#include "fs_xattr.hpp"
#include "fs_exists.hpp"
#include "fs_lgetxattr.hpp"
#include "fs_lsetxattr.hpp"
#include "str.hpp"

#include "scope_guard.hpp"

#include <array>
#include <cstring>


typedef std::array<char,64*1024> XATTRBuf;

static
std::string
_mergerfs_config_file(const fs::path &mount_)
{
  return mount_ / ".mergerfs";
}

static
int
_lgetxattr(const std::string &input_path_,
           const std::string &key_,
           std::string       *val_)
{
  std::string key;

  key = "user.mergerfs." + key_;

  return fs::xattr::get(input_path_,key,&val_);
}


bool
mergerfs::api::is_mergerfs(const fs::path &mountpoint_)
{
  fs::path cfgfile;

  cfgfile = ::_mergerfs_config_file(mountpoint_);

  return fs::exists(cfgfile);
}

int
mergerfs::api::get_kvs(const fs::path                    &mountpoint_,
                       std::map<std::string,std::string> *kvs_)
{
  fs::path cfgfile;

  cfgfile = ::_mergerfs_config_file(mountpoint_);

  return fs::xattr::get(cfgfile,kvs_);
}

int
mergerfs::api::set_kv(const fs::path    &mountpoint_,
                      const std::string &key_,
                      const std::string &val_)
{
  fs::path cfgfile;
  std::string xattr_key;


  cfgfile   = ::_mergerfs_config_file(mountpoint_);
  xattr_key = "user.mergerfs." + key_;

  return fs::xattr::set(cfgfile,key,val_,0);
}

int
mergerfs::api::allpaths(const std::string        &input_path_,
                        std::vector<std::string> *output_paths_)
{
  int rv;
  std::string val;

  rv = fs::xattr::get(input_path_,
                      "user.mergerfs.allpaths",
                      &val);
  if(rv < 0)
    return rv;

  str::split_on_null(val,&output_paths_);

  return 0;
}

int
mergerfs::api::basepath(const std::string &input_path_,
                        std::string       *basepath_)
{
  return fs::xattr::get(input_path_,
                        "user.mergerfs.basepath",
                        basepath_);
}

int
mergerfs::api::relpath(const std::string &input_path_,
                       std::string       *relpath_)
{
  return fs::xattr::get(input_path_,
                        "user.mergerfs.relpath",
                        relpath_);
}

int
mergerfs::api::fullpath(const std::string &input_path_,
                        std::string       &fullpath_)
{
  return ::_lgetxattr(input_path_,"fullpath",fullpath_);
}
