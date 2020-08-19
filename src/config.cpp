/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "config.hpp"
#include "ef.hpp"
#include "errno.hpp"
#include "from_string.hpp"
#include "num.hpp"
#include "rwlock.hpp"
#include "to_string.hpp"
#include "version.hpp"

#include <algorithm>
#include <string>
#include <iostream>

#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#define MINFREESPACE_DEFAULT (4294967295ULL)

using std::string;

#define IFERT(S) if(S == s_) return true

namespace l
{
  static
  bool
  readonly(const std::string &s_)
  {
    IFERT("async_read");
    IFERT("cache.symlinks");
    IFERT("cache.writeback");
    IFERT("fsname");
    IFERT("fuse_msg_size");
    IFERT("mount");
    IFERT("nullrw");
    IFERT("pid");
    IFERT("readdirplus");
    IFERT("threads");
    IFERT("version");

    return false;
  }
}

Config::Config()
  :
  open_cache(),

  controlfile("/.mergerfs"),

  async_read(true),
  auto_cache(false),
  branches(),
  cache_attr(1),
  cache_entry(1),
  cache_files(CacheFiles::ENUM::LIBFUSE),
  cache_negative_entry(0),
  cache_readdir(false),
  cache_statfs(0),
  cache_symlinks(false),
  category(func),
  direct_io(false),
  dropcacheonclose(false),
  fsname(),
  func(),
  fuse_msg_size(FUSE_MAX_MAX_PAGES),
  ignorepponrename(false),
  inodecalc("hybrid-hash"),
  link_cow(false),
  minfreespace(MINFREESPACE_DEFAULT),
  mount(),
  moveonenospc(false),
  nfsopenhack(NFSOpenHack::ENUM::OFF),
  nullrw(false),
  pid(::getpid()),
  posix_acl(false),
  readdir(ReadDir::ENUM::POSIX),
  readdirplus(false),
  security_capability(true),
  srcmounts(branches),
  statfs(StatFS::ENUM::BASE),
  statfs_ignore(StatFSIgnore::ENUM::NONE),
  symlinkify(false),
  symlinkify_timeout(3600),
  threads(0),
  version(MERGERFS_VERSION),
  writeback_cache(false),
  xattr(XAttr::ENUM::PASSTHROUGH)
{
  _map["async_read"]           = &async_read;
  _map["auto_cache"]           = &auto_cache;
  _map["branches"]             = &branches;
  _map["cache.attr"]           = &cache_attr;
  _map["cache.entry"]          = &cache_entry;
  _map["cache.files"]          = &cache_files;
  _map["cache.negative_entry"] = &cache_negative_entry;
  _map["cache.readdir"]        = &cache_readdir;
  _map["cache.statfs"]         = &cache_statfs;
  _map["cache.symlinks"]       = &cache_symlinks;
  _map["cache.writeback"]      = &writeback_cache;
  _map["category.action"]      = &category.action;
  _map["category.create"]      = &category.create;
  _map["category.search"]      = &category.search;
  _map["direct_io"]            = &direct_io;
  _map["dropcacheonclose"]     = &dropcacheonclose;
  _map["fsname"]               = &fsname;
  _map["func.access"]          = &func.access;
  _map["func.chmod"]           = &func.chmod;
  _map["func.chown"]           = &func.chown;
  _map["func.create"]          = &func.create;
  _map["func.getattr"]         = &func.getattr;
  _map["func.getxattr"]        = &func.getxattr;
  _map["func.link"]            = &func.link;
  _map["func.listxattr"]       = &func.listxattr;
  _map["func.mkdir"]           = &func.mkdir;
  _map["func.mknod"]           = &func.mknod;
  _map["func.open"]            = &func.open;
  _map["func.readlink"]        = &func.readlink;
  _map["func.removexattr"]     = &func.removexattr;
  _map["func.rename"]          = &func.rename;
  _map["func.rmdir"]           = &func.rmdir;
  _map["func.setxattr"]        = &func.setxattr;
  _map["func.symlink"]         = &func.symlink;
  _map["func.truncate"]        = &func.truncate;
  _map["func.unlink"]          = &func.unlink;
  _map["func.utimens"]         = &func.utimens;
  _map["fuse_msg_size"]        = &fuse_msg_size;
  _map["ignorepponrename"]     = &ignorepponrename;
  _map["inodecalc"]            = &inodecalc;
  _map["kernel_cache"]         = &kernel_cache;
  _map["link_cow"]             = &link_cow;
  _map["minfreespace"]         = &minfreespace;
  _map["mount"]                = &mount;
  _map["moveonenospc"]         = &moveonenospc;
  _map["nfsopenhack"]          = &nfsopenhack;
  _map["nullrw"]               = &nullrw;
  _map["pid"]                  = &pid;
  _map["posix_acl"]            = &posix_acl;
  //  _map["readdir"]              = &readdir;
  _map["readdirplus"]          = &readdirplus;
  _map["security_capability"]  = &security_capability;
  _map["srcmounts"]            = &srcmounts;
  _map["statfs"]               = &statfs;
  _map["statfs_ignore"]        = &statfs_ignore;
  _map["symlinkify"]           = &symlinkify;
  _map["symlinkify_timeout"]   = &symlinkify_timeout;
  _map["threads"]              = &threads;
  _map["version"]              = &version;
  _map["xattr"]                = &xattr;
}

const
Config&
Config::ro(void)
{
  return *((Config*)fuse_get_context()->private_data);
}

Config&
Config::rw(void)
{
  return *((Config*)fuse_get_context()->private_data);
}

bool
Config::has_key(const std::string &key_) const
{
  return _map.count(key_);
}

void
Config::keys(std::string &s_) const
{
  Str2TFStrMap::const_iterator i;
  Str2TFStrMap::const_iterator ei;

  s_.reserve(512);

  i  = _map.begin();
  ei = _map.end();

  for(; i != ei; ++i)
    {
      s_ += i->first;
      s_ += '\0';
    }

  s_.resize(s_.size() - 1);
}


void
Config::keys_xattr(std::string &s_) const
{
  Str2TFStrMap::const_iterator i;
  Str2TFStrMap::const_iterator ei;

  s_.reserve(1024);
  for(i = _map.begin(), ei = _map.end(); i != ei; ++i)
    {
      s_ += "user.mergerfs.";
      s_ += i->first;
      s_ += '\0';
    }
}

int
Config::get(const std::string &key_,
            std::string       *val_) const
{
  Str2TFStrMap::const_iterator i;

  i = _map.find(key_);
  if(i == _map.end())
    return -ENOATTR;

  *val_ = i->second->to_string();

  return 0;
}

int
Config::set_raw(const std::string &key_,
                const std::string &value_)
{
  Str2TFStrMap::iterator i;

  i = _map.find(key_);
  if(i == _map.end())
    return -ENOATTR;

  return i->second->from_string(value_);
}

int
Config::set(const std::string &key_,
            const std::string &value_)
{
  if(l::readonly(key_))
    return -EINVAL;

  return set_raw(key_,value_);
}

std::ostream&
operator<<(std::ostream &os_,
           const Config &c_)
{
  Str2TFStrMap::const_iterator i;
  Str2TFStrMap::const_iterator ei;

  for(i = c_._map.begin(), ei = c_._map.end(); i != ei; ++i)
    {
      os_ << i->first << '=' << i->second << '\n';
    }

  return os_;
}
