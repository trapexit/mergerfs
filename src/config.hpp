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

#pragma once

#include "branches.hpp"
#include "category.hpp"
#include "config_cachefiles.hpp"
#include "config_flushonclose.hpp"
#include "config_follow_symlinks.hpp"
#include "config_inodecalc.hpp"
#include "config_link_exdev.hpp"
#include "config_log_metrics.hpp"
#include "config_moveonenospc.hpp"
#include "config_nfsopenhack.hpp"
#include "config_pagesize.hpp"
#include "config_passthrough.hpp"
#include "config_pid.hpp"
#include "config_rename_exdev.hpp"
#include "config_set.hpp"
#include "config_statfs.hpp"
#include "config_statfsignore.hpp"
#include "config_xattr.hpp"
#include "enum.hpp"
#include "errno.hpp"
#include "funcs.hpp"
#include "fuse_readdir.hpp"
#include "policy.hpp"
#include "rwlock.hpp"
#include "tofrom_wrapper.hpp"

#include "fuse.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <sys/stat.h>

typedef ToFromWrapper<bool>                  ConfigBOOL;
typedef ToFromWrapper<uint64_t>              ConfigUINT64;
typedef ToFromWrapper<int>                   ConfigINT;
typedef ToFromWrapper<std::string>           ConfigSTR;
typedef ToFromWrapper<std::filesystem::path> ConfigPath;
typedef std::map<std::string,ToFromString*>  Str2TFStrMap;

extern const std::string CONTROLFILE;

class Config
{
public:
  struct Err
  {
    int err;
    std::string str;
  };

  typedef std::vector<Err> ErrVec;

public:
  class Read
  {
  public:
    Read();

  public:
    inline const Config* operator->() const;

  private:
    const Config &_cfg;
  };

public:
  class Write
  {
  public:
    Write();

  public:
    Config* operator->();

  private:
    Config &_cfg;
  };

public:
  Config();

public:
  Config& operator=(const Config&);

public:
  ConfigBOOL     async_read;
  ConfigBOOL     auto_cache;
  ConfigUINT64   minfreespace;
  Branches       branches;
  ConfigUINT64   branches_mount_timeout;
  ConfigBOOL     branches_mount_timeout_fail;
  ConfigUINT64   cache_attr;
  ConfigUINT64   cache_entry;
  CacheFiles     cache_files;
  ConfigSet      cache_files_process_names;
  ConfigUINT64   cache_negative_entry;
  ConfigBOOL     cache_readdir;
  ConfigUINT64   cache_statfs;
  ConfigBOOL     cache_symlinks;
  Categories     category;
  ConfigBOOL     direct_io;
  ConfigBOOL     direct_io_allow_mmap;
  ConfigBOOL     dropcacheonclose;
  ConfigBOOL     export_support;
  FlushOnClose   flushonclose;
  FollowSymlinks follow_symlinks;
  ConfigSTR      fsname;
  Funcs          func;
  ConfigPageSize fuse_msg_size;
  ConfigBOOL     handle_killpriv = true;
  ConfigBOOL     handle_killpriv_v2 = true;
  ConfigBOOL     ignorepponrename;
  InodeCalc      inodecalc;
  ConfigBOOL     kernel_cache;
  ConfigBOOL     kernel_permissions_check = true;
  ConfigBOOL     lazy_umount_mountpoint;
  ConfigBOOL     link_cow;
  LinkEXDEV      link_exdev;
  LogMetrics     log_metrics;
  ConfigPath     mountpoint;
  MoveOnENOSPC   moveonenospc;
  NFSOpenHack    nfsopenhack;
  ConfigBOOL     nullrw;
  Passthrough    passthrough = Passthrough::ENUM::OFF;
  ConfigBOOL     parallel_direct_writes;
  ConfigGetPid   pid;
  ConfigBOOL     posix_acl;
  ConfigUINT64   readahead;
  FUSE::ReadDir  readdir;
  ConfigBOOL     readdirplus;
  RenameEXDEV    rename_exdev;
  ConfigINT      scheduling_priority;
  ConfigBOOL     security_capability;
  SrcMounts      srcmounts;
  StatFS         statfs;
  StatFSIgnore   statfs_ignore;
  ConfigBOOL     symlinkify;
  ConfigUINT64   symlinkify_timeout;
  ConfigINT      fuse_read_thread_count;
  ConfigINT      fuse_process_thread_count;
  ConfigINT      fuse_process_thread_queue_depth;
  ConfigSTR      fuse_pin_threads;
  ConfigSTR      version;
  ConfigBOOL     writeback_cache;
  XAttr          xattr;

private:
  bool _initialized;

public:
  void finish_initializing();

public:
  friend std::ostream& operator<<(std::ostream &s,
                                  const Config &c);

public:
  bool has_key(const std::string &key) const;
  void keys(std::string &s) const;
  void keys_xattr(std::string &s) const;

public:
  int get(const std::string &key, std::string *val) const;
  int set_raw(const std::string &key, const std::string &val);
  int set(const std::string &key, const std::string &val);
  int set(const std::string &kv);

public:
  int from_stream(std::istream &istrm, ErrVec *errs);
  int from_file(const std::string &filepath, ErrVec *errs);

private:
  Str2TFStrMap _map;

private:
  static Config _singleton;

public:
  friend class Read;
  friend class Write;
};

std::ostream& operator<<(std::ostream &s,const Config::ErrVec &ev);

inline
Config::Read::Read()
  : _cfg(Config::_singleton)
{

}

inline
const
Config*
Config::Read::operator->() const
{
  return &_cfg;
}

inline
Config::Write::Write()
  : _cfg(Config::_singleton)
{

}

inline
Config*
Config::Write::operator->()
{
  return &_cfg;
}
