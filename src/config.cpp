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
#include "str.hpp"
#include "to_string.hpp"
#include "version.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MINFREESPACE_DEFAULT (4294967295ULL)

#define IFERT(S) if(S == s_) return true

const std::string CONTROLFILE = "/.mergerfs";
constexpr static const char CACHE_FILES_PROCESS_NAMES_DEFAULT[] =
  "rtorrent|"
  "qbittorrent-nox";


Config Config::_singleton;


namespace l
{
  static
  bool
  readonly(const std::string &s_)
  {
    IFERT("async_read");
    IFERT("branches-mount-timeout");
    IFERT("branches-mount-timeout-fail");
    IFERT("cache.symlinks");
    IFERT("cache.writeback");
    IFERT("direct-io-allow-mmap");
    IFERT("export-support");
    IFERT("fsname");
    IFERT("fuse_msg_size");
    IFERT("kernel-permissions-check");
    IFERT("mount");
    IFERT("nullrw");
    IFERT("pid");
    IFERT("pin-threads");
    IFERT("process-thread-count");
    IFERT("process-thread-queue-depth");
    IFERT("read-thread-count");
    IFERT("readdirplus");
    IFERT("scheduling-priority");
    IFERT("srcmounts");
    IFERT("threads");
    IFERT("version");

    return false;
  }
}

Config::Config()
  : async_read(true),
    auto_cache(false),
    minfreespace(MINFREESPACE_DEFAULT),
    branches(minfreespace),
    branches_mount_timeout(0),
    branches_mount_timeout_fail(false),
    cache_attr(1),
    cache_entry(1),
    cache_files(CacheFiles::ENUM::OFF),
    cache_files_process_names(CACHE_FILES_PROCESS_NAMES_DEFAULT),
    cache_negative_entry(0),
    cache_readdir(false),
    cache_statfs(0),
    cache_symlinks(false),
    category(func),
    direct_io(false),
    direct_io_allow_mmap(true),
    dropcacheonclose(false),
    export_support(true),
    flushonclose(FlushOnClose::ENUM::OPENED_FOR_WRITE),
    follow_symlinks(FollowSymlinks::ENUM::NEVER),
    fsname(),
    func(),
    fuse_msg_size("1M"),
    ignorepponrename(false),
    inodecalc("hybrid-hash"),
    lazy_umount_mountpoint(false),
    link_cow(false),
    link_exdev(LinkEXDEV::ENUM::PASSTHROUGH),
    log_metrics(false),
    mountpoint(),
    moveonenospc(true),
    nfsopenhack(NFSOpenHack::ENUM::OFF),
    nullrw(false),
    parallel_direct_writes(true),
    posix_acl(false),
    readahead(0),
    readdir("seq"),
    readdirplus(false),
    rename_exdev(RenameEXDEV::ENUM::PASSTHROUGH),
    scheduling_priority(-10),
    security_capability(true),
    srcmounts(branches),
    statfs(StatFS::ENUM::BASE),
    statfs_ignore(StatFSIgnore::ENUM::NONE),
    symlinkify(false),
    symlinkify_timeout(3600),
    fuse_read_thread_count(-1),
    fuse_process_thread_count(-1),
    fuse_process_thread_queue_depth(2),
    fuse_pin_threads("false"),
    version(MERGERFS_VERSION),
    writeback_cache(false),
    xattr(XAttr::ENUM::PASSTHROUGH),
    _initialized(false)
{
  _map["async_read"]             = &async_read;
  _map["auto_cache"]             = &auto_cache;
  _map["branches"]               = &branches;
  _map["branches-mount-timeout"] = &branches_mount_timeout;
  _map["branches-mount-timeout-fail"] = &branches_mount_timeout_fail;
  _map["cache.attr"]             = &cache_attr;
  _map["cache.entry"]            = &cache_entry;
  _map["cache.files"]            = &cache_files;
  _map["cache.files.process-names"] = &cache_files_process_names;
  _map["cache.negative_entry"]   = &cache_negative_entry;
  _map["cache.readdir"]          = &cache_readdir;
  _map["cache.statfs"]           = &cache_statfs;
  _map["cache.symlinks"]         = &cache_symlinks;
  _map["cache.writeback"]        = &writeback_cache;
  _map["category.action"]        = &category.action;
  _map["category.create"]        = &category.create;
  _map["category.search"]        = &category.search;
  _map["direct_io"]              = &direct_io;
  _map["direct-io-allow-mmap"]   = &direct_io_allow_mmap;
  _map["dropcacheonclose"]       = &dropcacheonclose;
  _map["export-support"]         = &export_support;
  _map["flush-on-close"]         = &flushonclose;
  _map["follow-symlinks"]        = &follow_symlinks;
  _map["fsname"]                 = &fsname;
  _map["func.access"]            = &func.access;
  _map["func.chmod"]             = &func.chmod;
  _map["func.chown"]             = &func.chown;
  _map["func.create"]            = &func.create;
  _map["func.getattr"]           = &func.getattr;
  _map["func.getxattr"]          = &func.getxattr;
  _map["func.link"]              = &func.link;
  _map["func.listxattr"]         = &func.listxattr;
  _map["func.mkdir"]             = &func.mkdir;
  _map["func.mknod"]             = &func.mknod;
  _map["func.open"]              = &func.open;
  _map["func.readdir"]           = &readdir;
  _map["func.readlink"]          = &func.readlink;
  _map["func.removexattr"]       = &func.removexattr;
  _map["func.rename"]            = &func.rename;
  _map["func.rmdir"]             = &func.rmdir;
  _map["func.setxattr"]          = &func.setxattr;
  _map["func.symlink"]           = &func.symlink;
  _map["func.truncate"]          = &func.truncate;
  _map["func.unlink"]            = &func.unlink;
  _map["func.utimens"]           = &func.utimens;
  _map["fuse_msg_size"]          = &fuse_msg_size;
  _map["ignorepponrename"]       = &ignorepponrename;
  _map["inodecalc"]              = &inodecalc;
  _map["kernel_cache"]           = &kernel_cache;
  _map["kernel-permissions-check"] = &kernel_permissions_check;
  _map["lazy-umount-mountpoint"] = &lazy_umount_mountpoint;
  _map["link_cow"]               = &link_cow;
  _map["link-exdev"]             = &link_exdev;
  _map["log.metrics"]            = &log_metrics;
  _map["minfreespace"]           = &minfreespace;
  _map["mount"]                  = &mountpoint;
  _map["moveonenospc"]           = &moveonenospc;
  _map["nfsopenhack"]            = &nfsopenhack;
  _map["nullrw"]                 = &nullrw;
  _map["pid"]                    = &pid;
  _map["parallel-direct-writes"] = &parallel_direct_writes;
  _map["pin-threads"]            = &fuse_pin_threads;
  _map["posix_acl"]              = &posix_acl;
  _map["readahead"]              = &readahead;
  _map["readdirplus"]            = &readdirplus;
  _map["rename-exdev"]           = &rename_exdev;
  _map["scheduling-priority"]    = &scheduling_priority;
  _map["security_capability"]    = &security_capability;
  _map["srcmounts"]              = &srcmounts;
  _map["statfs"]                 = &statfs;
  _map["statfs_ignore"]          = &statfs_ignore;
  _map["symlinkify"]             = &symlinkify;
  _map["symlinkify_timeout"]     = &symlinkify_timeout;
  _map["threads"]                = &fuse_read_thread_count;
  _map["read-thread-count"]      = &fuse_read_thread_count;
  _map["process-thread-count"]   = &fuse_process_thread_count;
  _map["process-thread-queue-depth"] = &fuse_process_thread_queue_depth;
  _map["version"]                = &version;
  _map["xattr"]                  = &xattr;
}

Config&
Config::operator=(const Config &cfg_)
{
  int rv;
  std::string val;

  for(auto &kv : _map)
    {
      rv = cfg_.get(kv.first,&val);
      if(rv)
        continue;

      kv.second->from_string(val);
    }

  return *this;
}

bool
Config::has_key(const std::string &key_) const
{
  return _map.count(key_);
}

void
Config::keys(std::string &s_) const
{
  s_.reserve(512);

  for(const auto &[key,val] : _map)
    {
      s_ += key;
      s_ += '\0';
    }

  s_.resize(s_.size() - 1);
}


void
Config::keys_xattr(std::string &s_) const
{
  s_.reserve(1024);
  for(const auto &[key,val] : _map)
    {
      s_ += "user.mergerfs.";
      s_ += key;
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
  if(_initialized && l::readonly(key_))
    return -EROFS;

  return set_raw(key_,value_);
}

int
Config::set(const std::string &kv_)
{
  std::string key;
  std::string val;

  str::splitkv(kv_,'=',&key,&val);
  key = str::trim(key);
  val = str::trim(val);

  return set(key,val);
}

int
Config::from_stream(std::istream &istrm_,
                    ErrVec       *errs_)
{
  int rv;
  std::string line;
  std::string key;
  std::string val;
  Config newcfg;

  newcfg = *this;

  while(std::getline(istrm_,line,'\n'))
    {
      line = str::trim(line);
      if(!line.empty() && (line[0] == '#'))
        continue;

      str::splitkv(line,'=',&key,&val);
      key = str::trim(key);
      val = str::trim(val);

      rv = newcfg.set(key,val);
      if(rv < 0)
        errs_->push_back({rv,key});
    }

  if(!errs_->empty())
    return -EINVAL;

  *this = newcfg;

  return 0;
}

int
Config::from_file(const std::string &filepath_,
                  ErrVec            *errs_)
{
  int rv;
  std::ifstream ifstrm;

  ifstrm.open(filepath_);
  if(!ifstrm.good())
    {
      errs_->push_back({-errno,filepath_});
      return -errno;
    }

  rv = from_stream(ifstrm,errs_);

  ifstrm.close();

  return rv;
}

void
Config::finish_initializing()
{
  _initialized = true;
}

std::ostream&
operator<<(std::ostream &os_,
           const Config &c_)
{
  for(const auto &[key,val] : c_._map)
    os_ << key << '=' << val->to_string() << std::endl;

  return os_;
}


static
std::string
err2str(const int err_)
{
  switch(err_)
    {
    case 0:
      return std::string();
    case -EINVAL:
      return "invalid value";
    case -ENOATTR:
      return "unknown option";
    case -EROFS:
      return "read-only option";
    default:
      return strerror(-err_);
    }

  return std::string();
}

std::ostream&
operator<<(std::ostream         &os_,
           const Config::ErrVec &ev_)
{
  std::string errstr;

  for(auto &err : ev_)
    {
      os_ << "* ERROR: ";
      errstr = err2str(err.err);
      if(!errstr.empty())
        os_ << errstr << " - ";
      os_ << err.str << std::endl;
    }

  return os_;
}
