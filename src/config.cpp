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
#include "fs_path.hpp"
#include "nonstd/string.hpp"
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


constexpr static const char CACHE_FILES_PROCESS_NAMES_DEFAULT[] =
  "rtorrent|"
  "qbittorrent-nox";

Config cfg;

Config::CfgConfigFile::CfgConfigFile()
{
}

int
Config::CfgConfigFile::from_string(const std::string_view s_)
{
  int rv;
  fs::path cfg_file;

  if(_depth > 5)
    return -ELOOP;
  _depth++;

  cfg_file = (s_.empty() ? _cfg_file : s_);

  rv = cfg.from_file(cfg_file);
  if(rv == 0)
    _cfg_file = cfg_file;

  _depth--;

  return rv;
}

std::string
Config::CfgConfigFile::to_string() const
{
  return _cfg_file;
}


Config::Config()
  :
  allow_idmap(false),
  async_read(true),
  branches(),
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
  cache_writeback(false),
  config_file(),
  direct_io_allow_mmap(true),
  dropcacheonclose(false),
  export_support(true),
  flushonclose(FlushOnClose::ENUM::OPENED_FOR_WRITE),
  follow_symlinks(FollowSymlinks::ENUM::NEVER),
  fsname(),
  category(func),
  fuse_msg_size("1M"),
  gid_cache_expire_timeout(60 * 60),
  gid_cache_remove_timeout(60 * 60 * 12),
  handle_killpriv(true),
  handle_killpriv_v2(true),
  ignorepponrename(false),
  inodecalc("hybrid-hash"),
  kernel_permissions_check(true),
  lazy_umount_mountpoint(false),
  link_cow(false),
  link_exdev(LinkEXDEV::ENUM::PASSTHROUGH),
  log_metrics(false),
  minfreespace(branches.minfreespace),
  mountpoint(),
  moveonenospc(true),
  nfsopenhack(NFSOpenHack::ENUM::OFF),
  nullrw(false),
  parallel_direct_writes(true),
  passthrough_io(PassthroughIO::ENUM::OFF),
  passthrough_max_stack_depth(fuse_cfg.passthrough_max_stack_depth),
  pin_threads(fuse_cfg.pin_threads),
  posix_acl(false),
  process_thread_count(fuse_cfg.process_thread_count),
  process_thread_queue_depth(fuse_cfg.process_thread_queue_depth),
  proxy_ioprio(false),
  read_thread_count(fuse_cfg.read_thread_count),
  readahead(0),
  readdir("seq"),
  rename_exdev(RenameEXDEV::ENUM::PASSTHROUGH),
  scheduling_priority(-10),
  security_capability(true),
  statfs(StatFS::ENUM::BASE),
  statfs_ignore(StatFSIgnore::ENUM::NONE),
  symlinkify(false),
  symlinkify_timeout(3600),
  xattr(XAttr::ENUM::PASSTHROUGH),

  _congestion_threshold(fuse_cfg.congestion_threshold),
  _debug(fuse_cfg.debug),
  _gid(fuse_cfg.gid),
  _max_background(fuse_cfg.max_background),
  _mount(mountpoint),
  _mountpoint(mountpoint),
  _never_forget_nodes(),
  _noforget(),
  _remember(fuse_cfg.remember_nodes),
  _remember_nodes(fuse_cfg.remember_nodes),
  _srcmounts(branches),
  _threads(fuse_cfg.read_thread_count),
  _uid(fuse_cfg.uid),
  _umask(fuse_cfg.umask),
  _version(MERGERFS_VERSION),

  _initialized(false)
{
  allow_idmap.ro =
    async_read.ro =
    branches_mount_timeout.ro =
    branches_mount_timeout_fail.ro =
    cache_symlinks.ro =
    cache_writeback.ro =
    _congestion_threshold.ro =
    direct_io_allow_mmap.ro =
    export_support.ro =
    fsname.ro =
    fuse_msg_size.ro =
    handle_killpriv.ro =
    handle_killpriv_v2.ro =
    kernel_permissions_check.ro =
    _max_background.ro =
    _mount.ro =
    _mountpoint.ro =
    nullrw.ro =
    pid.ro =
    pin_threads.ro =
    posix_acl.ro =
    process_thread_count.ro =
    process_thread_queue_depth.ro =
    read_thread_count.ro =
    scheduling_priority.ro =
    _srcmounts.ro =
    _version.ro =
    true;
  _congestion_threshold.display =
    _gid.display =
    _max_background.display =
    _mount.display =
    _noforget.display =
    _remember.display =
    _srcmounts.display =
    _threads.display =
    _uid.display =
    _umask.display =
    false;

  _map["allow-idmap"]                 = &allow_idmap;
  _map["async-read"]                  = &async_read;
  _map["atomic-o-trunc"]              = &_dummy;
  _map["auto-cache"]                  = &_dummy;
  _map["big-writes"]                  = &_dummy;
  _map["branches"]                    = &branches;
  _map["branches-mount-timeout"]      = &branches_mount_timeout;
  _map["branches-mount-timeout-fail"] = &branches_mount_timeout_fail;
  _map["cache.attr"]                  = &cache_attr;
  _map["cache.entry"]                 = &cache_entry;
  _map["cache.files"]                 = &cache_files;
  _map["cache.files.process-names"]   = &cache_files_process_names;
  _map["cache.negative-entry"]        = &cache_negative_entry;
  _map["cache.open"]                  = &_dummy;
  _map["cache.readdir"]               = &cache_readdir;
  _map["cache.statfs"]                = &cache_statfs;
  _map["cache.symlinks"]              = &cache_symlinks;
  _map["cache.writeback"]             = &cache_writeback;
  _map["category.action"]             = &category.action;
  _map["category.create"]             = &category.create;
  _map["category.search"]             = &category.search;
  _map["config"]                      = &config_file;
  _map["debug"]                       = &_debug;
  _map["defaults"]                    = &_dummy;
  _map["direct-io"]                   = &_dummy;
  _map["direct-io-allow-mmap"]        = &direct_io_allow_mmap;
  _map["dropcacheonclose"]            = &dropcacheonclose;
  _map["export-support"]              = &export_support;
  _map["flush-on-close"]              = &flushonclose;
  _map["follow-symlinks"]             = &follow_symlinks;
  _map["fsname"]                      = &fsname;
  _map["func.access"]                 = &func.access;
  _map["func.chmod"]                  = &func.chmod;
  _map["func.chown"]                  = &func.chown;
  _map["func.create"]                 = &func.create;
  _map["func.getattr"]                = &getattr;
  _map["func.getxattr"]               = &func.getxattr;
  _map["func.link"]                   = &func.link;
  _map["func.listxattr"]              = &func.listxattr;
  _map["func.mkdir"]                  = &func.mkdir;
  _map["func.mknod"]                  = &func.mknod;
  _map["func.open"]                   = &func.open;
  _map["func.readdir"]                = &readdir;
  _map["func.readlink"]               = &func.readlink;
  _map["func.removexattr"]            = &func.removexattr;
  _map["func.rename"]                 = &func.rename;
  _map["func.rmdir"]                  = &func.rmdir;
  _map["func.setxattr"]               = &func.setxattr;
  _map["func.statx"]                  = &statx;
  _map["func.symlink"]                = &func.symlink;
  _map["func.truncate"]               = &func.truncate;
  _map["func.unlink"]                 = &func.unlink;
  _map["func.utimens"]                = &func.utimens;
  _map["fuse-msg-size"]               = &fuse_msg_size;
  _map["gid"]                         = &_gid;
  _map["gid-cache.expire-timeout"]    = &gid_cache_expire_timeout;
  _map["gid-cache.remove-timeout"]    = &gid_cache_remove_timeout;
  _map["handle-killpriv"]             = &handle_killpriv;
  _map["handle-killpriv-v2"]          = &handle_killpriv_v2;
  _map["hard-remove"]                 = &_dummy;
  _map["ignorepponrename"]            = &ignorepponrename;
  _map["inodecalc"]                   = &inodecalc;
  _map["kernel-cache"]                = &_dummy;
  _map["kernel-permissions-check"]    = &kernel_permissions_check;
  _map["lazy-umount-mountpoint"]      = &lazy_umount_mountpoint;
  _map["link-cow"]                    = &link_cow;
  _map["link-exdev"]                  = &link_exdev;
  _map["log.metrics"]                 = &log_metrics;
  _map["minfreespace"]                = &minfreespace;
  _map["mount"]                       = &_mount;
  _map["mountpoint"]                  = &_mountpoint;
  _map["moveonenospc"]                = &moveonenospc;
  _map["negative-entry"]              = &_dummy;
  _map["never-forget-nodes"]          = &_never_forget_nodes;
  _map["nfsopenhack"]                 = &nfsopenhack;
  _map["no-splice-move"]              = &_dummy;
  _map["no-splice-read"]              = &_dummy;
  _map["no-splice-write"]             = &_dummy;
  _map["noforget"]                    = &_noforget;
  _map["nonempty"]                    = &_dummy;
  _map["nullrw"]                      = &nullrw;
  _map["parallel-direct-writes"]      = &parallel_direct_writes;
  _map["passthrough.io"]              = &passthrough_io;
  _map["passthrough.max-stack-depth"] = &passthrough_max_stack_depth;
  _map["pid"]                         = &pid;
  _map["pin-threads"]                 = &pin_threads;
  _map["posix-acl"]                   = &posix_acl;
  _map["process-thread-count"]        = &process_thread_count;
  _map["process-thread-queue-depth"]  = &process_thread_queue_depth;
  _map["proxy-ioprio"]                = &proxy_ioprio;
  _map["read-thread-count"]           = &read_thread_count;
  _map["readahead"]                   = &readahead;
  _map["remember"]                    = &_remember;
  _map["remember-nodes"]              = &_remember_nodes;
  _map["rename-exdev"]                = &rename_exdev;
  _map["scheduling-priority"]         = &scheduling_priority;
  _map["security-capability"]         = &security_capability;
  _map["splice-move"]                 = &_dummy;
  _map["splice-read"]                 = &_dummy;
  _map["splice-write"]                = &_dummy;
  _map["srcmounts"]                   = &_srcmounts;
  _map["statfs"]                      = &statfs;
  _map["statfs-ignore"]               = &statfs_ignore;
  _map["symlinkify"]                  = &symlinkify;
  _map["symlinkify-timeout"]          = &symlinkify_timeout;
  _map["threads"]                     = &_threads;
  _map["uid"]                         = &_uid;
  _map["umask"]                       = &_umask;
  _map["use-ino"]                     = &_dummy;
  _map["version"]                     = &_version;
  _map["xattr"]                       = &xattr;
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
      if(val->display == false)
        continue;

      s_ += "user.mergerfs.";
      s_ += key;
      s_ += '\0';
    }
}

ssize_t
Config::keys_listxattr_size() const
{
  ssize_t rv;

  rv = 0;
  for(const auto &[key,val] : _map)
    {
      if(val->display == false)
        continue;

      rv += sizeof("user.mergerfs.");
      rv += key.size();
    }

  return rv;
}

ssize_t
Config::keys_listxattr(char   *list_,
                       size_t  size_) const
{
  char    *list = list_;
  ssize_t  size = size_;

  if(size_ == 0)
    return keys_listxattr_size();

  for(const auto &[key,val] : _map)
    {
      if(val->display == false)
        continue;

      auto rv = fmt::format_to_n(list,size,
                                 "user.mergerfs.{}\0",
                                 key);
      if(rv.out > (list + size))
        return -ERANGE;
      list += rv.size;
      size -= rv.size;
    }

  return (list - list_);
}

int
Config::get(const std::string &key_,
            std::string       *val_) const
{
  std::string key;
  Str2TFStrMap::const_iterator i;

  key = str::replace(key_,'_','-');


  i = _map.find(key);
  if(i == _map.end())
    return -ENOATTR;

  *val_ = i->second->to_string();

  return 0;
}

int
Config::set(const std::string &key_,
            const std::string &value_)
{
  std::string key;
  Str2TFStrMap::iterator i;

  key = str::replace(key_,'_','-');

  i = _map.find(key);
  if(i == _map.end())
    return -ENOATTR;
  if(_initialized && i->second->ro)
    return -EROFS;

  return i->second->from_string(value_);
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
Config::from_stream(std::istream &istrm_)
{
  int rv;
  std::string line;
  Config::ErrVec new_errs;

  while(std::getline(istrm_,line,'\n'))
    {
      line = str::trim(line);
      if(line.empty() || (line[0] == '#'))
        continue;

      rv = set(line);
      if(rv < 0)
        new_errs.push_back({-rv,line});
    }

  rv = (new_errs.empty() ? 0 : -EINVAL);
  errs.insert(errs.end(),
              new_errs.begin(),
              new_errs.end());

  return rv;
}

int
Config::from_file(const std::string &filepath_)
{
  int rv;
  std::ifstream ifstrm;

  ifstrm.open(filepath_);
  if(!ifstrm.good())
    {
      errs.push_back({errno,filepath_});
      return -errno;
    }

  rv = from_stream(ifstrm);

  ifstrm.close();

  return rv;
}

void
Config::finish_initializing()
{
  _initialized = true;
}

bool
Config::is_rootdir(const fs::path &fusepath_)
{
  return fusepath_.empty();
}

bool
Config::is_ctrl_file(const fs::path &fusepath_)
{
  return (fusepath_ == ".mergerfs");
}

bool
Config::is_mergerfs_xattr(const char *attrname_)
{
  return str::startswith(attrname_,"user.mergerfs.");
}

bool
Config::is_cmd_xattr(const std::string_view &attrname_)
{
  return nonstd::string::starts_with(attrname_,"user.mergerfs.cmd.");
}

std::string
Config::prune_ctrl_xattr(const std::string &s_)
{
  const size_t offset = (sizeof("user.mergerfs.") - 1);

  if(offset < s_.size())
    return s_.substr(offset);
  return {};
}

std::string_view
Config::prune_cmd_xattr(const std::string_view &s_)
{
  constexpr size_t offset = (sizeof("user.mergerfs.cmd.") - 1);

  if(offset < s_.size())
    return s_.substr(offset);
  return {};
}

std::string
Config::Err::to_string() const
{
  std::string s;

  switch(err)
    {
    case 0:
      break;
    case EINVAL:
      s = "invalid value";
      break;
    case ENOATTR:
      s = "unknown option";
      break;
    case EROFS:
      s = "read-only option";
      break;
    case ELOOP:
      s = "too many levels of config";
      break;
    case ENOENT:
      s = "no value set";
      break;
    default:
      s = strerror(err);
      break;
    }

  return fmt::format("{} - {}",s,str);
}
