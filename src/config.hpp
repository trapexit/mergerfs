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

#include "branch.hpp"
#include "config_inodecalc.hpp"
#include "config_readdir.hpp"
#include "config_moveonenospc.hpp"
#include "enum.hpp"
#include "errno.hpp"
#include "func_category.hpp"
#include "funcs.hpp"
#include "policy.hpp"
#include "policy_cache.hpp"
#include "tofrom_wrapper.hpp"

#include <fuse.h>

#include <string>
#include <vector>

#include <stdint.h>
#include <sys/stat.h>

typedef ToFromWrapper<bool>                 ConfigBOOL;
typedef ToFromWrapper<uint64_t>             ConfigUINT64;
typedef ToFromWrapper<int>                  ConfigINT;
typedef ToFromWrapper<std::string>          ConfigSTR;
typedef std::map<std::string,ToFromString*> Str2TFStrMap;

class Config
{
public:
  enum class StatFSEnum
    {
      BASE,
      FULL
    };
  typedef Enum<StatFSEnum> StatFS;

  enum class StatFSIgnoreEnum
    {
      NONE,
      RO,
      NC
    };
  typedef Enum<StatFSIgnoreEnum> StatFSIgnore;

  enum class CacheFilesEnum
    {
      LIBFUSE,
      OFF,
      PARTIAL,
      FULL,
      AUTO_FULL
    };
  typedef Enum<CacheFilesEnum> CacheFiles;

  enum class XAttrEnum
    {
      PASSTHROUGH = 0,
      NOSYS       = ENOSYS,
      NOATTR      = ENOATTR
    };
  typedef Enum<XAttrEnum> XAttr;

public:
  Config();

public:
  mutable PolicyCache open_cache;

public:
  const std::string controlfile;

public:
  ConfigBOOL     async_read;
  ConfigBOOL     auto_cache;
  Branches       branches;
  ConfigUINT64   cache_attr;
  ConfigUINT64   cache_entry;
  CacheFiles     cache_files;
  ConfigUINT64   cache_negative_entry;
  ConfigBOOL     cache_readdir;
  ConfigUINT64   cache_statfs;
  ConfigBOOL     cache_symlinks;
  FuncCategories category;
  ConfigBOOL     direct_io;
  ConfigBOOL     dropcacheonclose;
  ConfigSTR      fsname;
  Funcs          func;
  ConfigUINT64   fuse_msg_size;
  ConfigBOOL     ignorepponrename;
  InodeCalc      inodecalc;
  ConfigBOOL     kernel_cache;
  ConfigBOOL     link_cow;
  ConfigUINT64   minfreespace;
  ConfigSTR      mount;
  MoveOnENOSPC   moveonenospc;
  ConfigBOOL     nullrw;
  ConfigUINT64   pid;
  ConfigBOOL     posix_acl;
  ReadDir        readdir;
  ConfigBOOL     readdirplus;
  ConfigBOOL     security_capability;
  SrcMounts      srcmounts;
  StatFS         statfs;
  StatFSIgnore   statfs_ignore;
  ConfigBOOL     symlinkify;
  ConfigUINT64   symlinkify_timeout;
  ConfigINT      threads;
  ConfigSTR      version;
  ConfigBOOL     writeback_cache;
  XAttr          xattr;

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

public:
  static const Config &ro(void);
  static Config       &rw(void);

private:
  Str2TFStrMap _map;
};
