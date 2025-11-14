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

#include "func_access.hpp"
#include "func_chmod.hpp"
#include "func_chown.hpp"
#include "func_getattr.hpp"
#include "func_getxattr.hpp"
#include "func_listxattr.hpp"
#include "func_readlink.hpp"
#include "func_removexattr.hpp"
#include "func_rmdir.hpp"
#include "func_setxattr.hpp"
#include "func_statx.hpp"
#include "func_truncate.hpp"
#include "func_unlink.hpp"
#include "func_utimens.hpp"

#include "branches.hpp"
#include "category.hpp"
#include "config_cachefiles.hpp"
#include "config_dummy.hpp"
#include "config_flushonclose.hpp"
#include "config_follow_symlinks.hpp"
#include "config_getattr_statx.hpp"
#include "config_inodecalc.hpp"
#include "config_link_exdev.hpp"
#include "config_log_metrics.hpp"
#include "config_moveonenospc.hpp"
#include "config_nfsopenhack.hpp"
#include "config_noforget.hpp"
#include "config_pagesize.hpp"
#include "config_passthrough_io.hpp"
#include "config_pid.hpp"
#include "config_proxy_ioprio.hpp"
#include "config_rename_exdev.hpp"
#include "config_set.hpp"
#include "config_statfs.hpp"
#include "config_statfsignore.hpp"
#include "config_xattr.hpp"
#include "enum.hpp"
#include "errno.hpp"
#include "fs_path.hpp"
#include "funcs.hpp"
#include "fuse_cfg.hpp"
#include "fuse_readdir.hpp"
#include "policy.hpp"
#include "rwlock.hpp"
#include "tofrom_ref.hpp"
#include "tofrom_wrapper.hpp"
#include "syslog.hpp"

#include "fuse.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <sys/stat.h>

typedef ToFromWrapper<bool>                 ConfigBOOL;
typedef ToFromWrapper<uint64_t>             ConfigUINT64;
typedef ToFromWrapper<int64_t>              ConfigS64;
typedef ToFromWrapper<int>                  ConfigINT;
typedef ToFromWrapper<std::string>          ConfigSTR;
typedef ToFromWrapper<fs::path>             ConfigPath;
typedef std::map<std::string,ToFromString*> Str2TFStrMap;
typedef ROToFromWrapper<std::string>        ConfigROSTR;

extern const std::string CONTROLFILE;

class Config
{
public:
  struct Err
  {
    int err;
    std::string str;

    std::string to_string() const;
  };

  typedef std::vector<Err> ErrVec;

public:
  ErrVec errs;

public:
  class CfgConfigFile : public ToFromString
  {
  private:
    fs::path  _cfg_file;
    int       _depth = 0;

  public:
    CfgConfigFile();

  public:
    int from_string(const std::string_view s_) final;
    std::string to_string(void) const final;
  };

public:
  Config();

public:
  Config& operator=(const Config&);

public:
  Func2::Access      access{"all"};
  Func2::Chmod       chmod{"all"};
  Func2::Chown       chown{"all"};
  Func2::GetAttr     getattr{"cdfo"};
  Func2::Getxattr    getxattr{"ff"};
  Func2::Listxattr   listxattr{"ff"};
  Func2::Readlink    readlink{"ff"};
  Func2::Removexattr removexattr{"all"};
  Func2::Rmdir       rmdir{"all"};
  Func2::Setxattr    setxattr{"all"};
  Func2::Statx       statx{"cdfo"};
  Func2::Truncate    truncate{"all"};
  Func2::Unlink      unlink{"all"};
  Func2::Utimens     utimens{"all"};

  ConfigBOOL     allow_idmap;
  ConfigBOOL     async_read;
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
  ConfigBOOL     cache_writeback;
  Categories     category;
  CfgConfigFile  config_file;
  ConfigBOOL     direct_io_allow_mmap;
  ConfigBOOL     dropcacheonclose;
  ConfigBOOL     export_support;
  FlushOnClose   flushonclose;
  FollowSymlinks follow_symlinks;
  ConfigSTR      fsname;
  Funcs          func;
  ConfigPageSize fuse_msg_size;
  ConfigBOOL     handle_killpriv;
  ConfigBOOL     handle_killpriv_v2;
  ConfigBOOL     ignorepponrename;
  InodeCalc      inodecalc;
  ConfigBOOL     kernel_permissions_check;
  ConfigBOOL     lazy_umount_mountpoint;
  ConfigBOOL     link_cow;
  LinkEXDEV      link_exdev;
  LogMetrics     log_metrics;
  TFSRef<u64>    minfreespace;
  fs::path       mountpoint;
  MoveOnENOSPC   moveonenospc;
  NFSOpenHack    nfsopenhack;
  ConfigBOOL     nullrw;
  ConfigBOOL     parallel_direct_writes;
  PassthroughIO  passthrough_io;
  TFSRef<int>    passthrough_max_stack_depth;
  ConfigGetPid   pid;
  TFSRef<std::string> pin_threads;
  ConfigBOOL     posix_acl;
  TFSRef<int>    process_thread_count;
  TFSRef<int>    process_thread_queue_depth;
  ProxyIOPrio    proxy_ioprio;
  TFSRef<int>    read_thread_count;
  ConfigUINT64   readahead;
  FUSE::ReadDir  readdir;
  RenameEXDEV    rename_exdev;
  ConfigINT      scheduling_priority;
  ConfigBOOL     security_capability;
  StatFS         statfs;
  StatFSIgnore   statfs_ignore;
  ConfigBOOL     symlinkify;
  ConfigS64      symlinkify_timeout;
  XAttr          xattr;

private:
  TFSRef<int>      _congestion_threshold;
  TFSRef<bool>     _debug;
  CfgDummy         _dummy;
  ConfigGetAttrStatx _getattr_statx;
  TFSRef<s64>      _gid;
  TFSRef<int>      _max_background;
  TFSRef<fs::path> _mount;
  TFSRef<fs::path> _mountpoint;
  CfgNoforget      _never_forget_nodes;
  CfgNoforget      _noforget;
  TFSRef<s64>      _remember;
  TFSRef<s64>      _remember_nodes;
  SrcMounts        _srcmounts;
  TFSRef<int>      _threads;
  TFSRef<s64>      _uid;
  TFSRef<s64>      _umask;
  ConfigROSTR      _version;

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
  ssize_t keys_listxattr(char *list, size_t size) const;
  ssize_t keys_listxattr_size() const;

public:
  int get(const std::string &key, std::string *val) const;
  int set(const std::string &key, const std::string &val);
  int set(const std::string &kv);

public:
  int from_stream(std::istream &istrm);
  int from_file(const std::string &filepath);

public:
  static bool is_rootdir(const fs::path &fusepath);
  static bool is_ctrl_file(const fs::path &fusepath);
  static bool is_mergerfs_xattr(const char *attrname);
  static bool is_cmd_xattr(const std::string_view &attrname);
  static std::string prune_ctrl_xattr(const std::string &s);
  static std::string_view prune_cmd_xattr(const std::string_view &s);

private:
  Str2TFStrMap _map;
};

std::ostream& operator<<(std::ostream &s,const Config::ErrVec &ev);

extern Config cfg;
