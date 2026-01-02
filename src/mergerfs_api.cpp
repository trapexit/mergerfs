/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "mergerfs_api.hpp"

#include "fs_exists.hpp"
#include "fs_xattr.hpp"
#include "str.hpp"

#include "scope_guard/scope_guard.hpp"

#include <array>
#include <cstring>


static
std::string
_mergerfs_config_file(const fs::path &mount_)
{
  return mount_ / ".mergerfs";
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
  int rv;
  fs::path cfgfile;
  std::map<std::string,std::string> kvs;

  cfgfile = ::_mergerfs_config_file(mountpoint_);

  rv = fs::xattr::get(cfgfile,&kvs);
  if(rv < 0)
    return rv;

  for(auto &[k,v] : kvs)
    {
      std::string key;

      key = str::remove_prefix(k,"user.mergerfs.");

      kvs_->insert({key,v});
    }

  return 0;
}

int
mergerfs::api::get_kv(const fs::path    &mountpoint_,
                      const std::string &key_,
                      std::string       *val_)
{
  fs::path cfgfile;
  std::string xattr_key;

  cfgfile = ::_mergerfs_config_file(mountpoint_);

  xattr_key = "user.mergerfs." + key_;

  return fs::xattr::get(cfgfile,xattr_key,val_);
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

  return fs::xattr::set(cfgfile,xattr_key,val_,0);
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

  *output_paths_ = str::split_on_null(val);

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
                        std::string       *fullpath_)
{
  return fs::xattr::get(input_path_,
                        "user.mergerfs.fullpath",
                        fullpath_);
}
