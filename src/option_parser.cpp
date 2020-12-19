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
#include "fs_glob.hpp"
#include "fs_statvfs_cache.hpp"
#include "hw_cpu.hpp"
#include "num.hpp"
#include "policy.hpp"
#include "str.hpp"
#include "version.hpp"

#include "fuse.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using std::string;
using std::vector;

enum
  {
    MERGERFS_OPT_HELP,
    MERGERFS_OPT_VERSION
  };


namespace l
{
  static
  int
  calculate_thread_count(int threads_)
  {
    int tc;

    if(threads_ > 0)
      return threads_;

    tc = hw::cpu::logical_core_count();
    if(threads_ < 0)
      tc /= -threads_;
    if(tc == 0)
      tc = 1;

    return tc;
  }
}

static
void
set_option(const std::string &option_,
           fuse_args         *args_)
{

  fuse_opt_add_arg(args_,"-o");
  fuse_opt_add_arg(args_,option_.c_str());
}

static
void
set_kv_option(const std::string &key_,
              const std::string &val_,
              fuse_args         *args_)
{
  std::string option;

  option = key_ + '=' + val_;

  set_option(option,args_);
}

static
void
set_threads(Config::Write &cfg_,
            fuse_args     *args_)
{
  int threads;

  threads = l::calculate_thread_count(cfg_->threads);

  cfg_->threads = threads;

  set_kv_option("threads",cfg_->threads.to_string(),args_);
}

static
void
set_fsname(Config::Write &cfg_,
           fuse_args     *args_)
{
  if(cfg_->fsname->empty())
    {
      vector<string> paths;

      cfg_->branches->to_paths(paths);

      if(paths.size() > 0)
        cfg_->fsname = str::remove_common_prefix_and_join(paths,':');
    }

  set_kv_option("fsname",cfg_->fsname,args_);
}

static
void
set_subtype(fuse_args *args_)
{
  set_kv_option("subtype","mergerfs",args_);
}

static
void
set_default_options(fuse_args *args_)
{
  set_option("default_permissions",args_);
}

static
int
parse_and_process_kv_arg(Config::Write     &cfg_,
                         Config::ErrVec    *errs_,
                         const std::string &key_,
                         const std::string &val_)
{
  int rv;
  std::string key(key_);
  std::string val(val_);

  rv = 0;
  if(key == "config")
    return ((cfg_->from_file(val_,errs_) < 0) ? 1 : 0);
  ef(key == "attr_timeout")
    key = "cache.attr";
  ef(key == "entry_timeout")
    key = "cache.entry";
  ef(key == "negative_entry")
    key = "cache.negative_entry";
  ef(key == "direct_io" && val.empty())
    val = "true";
  ef(key == "kernel_cache" && val.empty())
    val = "true";
  ef(key == "auto_cache" && val.empty())
    val = "true";
  ef(key == "async_read" && val.empty())
    val = "true";
  ef(key == "sync_read" && val.empty())
    {key = "async_read", val = "false";}
  ef(key == "defaults")
    return 0;
  ef(key == "hard_remove")
    return 0;
  ef(key == "atomic_o_trunc")
    return 0;
  ef(key == "big_writes")
    return 0;
  ef(key == "cache.open")
    return 0;

  if(cfg_->has_key(key) == false)
    return 1;

  rv = cfg_->set_raw(key,val);
  if(rv)
    errs_->push_back({rv,key+'='+val});

  return 0;
}

static
int
process_opt(Config::Write     &cfg_,
            Config::ErrVec    *errs_,
            const std::string &arg_)
{
  std::string key;
  std::string val;

  str::splitkv(arg_,'=',&key,&val);
  key = str::trim(key);
  val = str::trim(val);

  return parse_and_process_kv_arg(cfg_,errs_,key,val);
}

static
int
process_branches(Config::Write  &cfg_,
                 Config::ErrVec *errs_,
                 const char     *arg_)
{
  int rv;
  string arg;

  arg = arg_;
  rv = cfg_->set_raw("branches",arg);
  if(rv)
    errs_->push_back({rv,"branches="+arg});

  return 0;
}

static
int
process_mount(Config::Write  &cfg_,
              Config::ErrVec *errs_,
              const char     *arg_)
{
  int rv;
  string arg;

  arg = arg_;
  rv = cfg_->set_raw("mount",arg);
  if(rv)
    errs_->push_back({rv,"mount="+arg});

  return 1;
}

static
void
usage(void)
{
  std::cout <<
    "Usage: mergerfs [options] <branches> <destpath>\n"
    "\n"
    "    -o [opt,...]           mount options\n"
    "    -h --help              print help\n"
    "    -v --version           print version\n"
    "\n"
    "mergerfs options:\n"
    "    <branches>             ':' delimited list of directories. Supports\n"
    "                           shell globbing (must be escaped in shell)\n"
    "    -o config=FILE         Read options from file in key=val format\n"
    "    -o func.FUNC=POLICY    Set function FUNC to policy POLICY\n"
    "    -o category.CAT=POLICY Set functions in category CAT to POLICY\n"
    "    -o fsname=STR          Sets the name of the filesystem.\n"
    "    -o cache.open=INT      'open' policy cache timeout in seconds.\n"
    "                           default = 0 (disabled)\n"
    "    -o cache.statfs=INT    'statfs' cache timeout in seconds. Used by\n"
    "                           policies. default = 0 (disabled)\n"
    "    -o cache.files=libfuse|off|partial|full|auto-full\n"
    "                           * libfuse: Use direct_io, kernel_cache, auto_cache\n"
    "                             values directly\n"
    "                           * off: Disable page caching\n"
    "                           * partial: Clear page cache on file open\n"
    "                           * full: Keep cache on file open\n"
    "                           * auto-full: Keep cache if mtime & size not changed\n"
    "                           default = libfuse\n"
    "    -o cache.writeback=BOOL\n"
    "                           Enable kernel writeback caching (if supported)\n"
    "                           cache.files must be enabled as well.\n"
    "                           default = false\n"
    "    -o cache.symlinks=BOOL\n"
    "                           Enable kernel caching of symlinks (if supported)\n"
    "                           default = false\n"
    "    -o cache.readdir=BOOL\n"
    "                           Enable kernel caching readdir (if supported)\n"
    "                           default = false\n"
    "    -o cache.attr=INT      File attribute cache timeout in seconds.\n"
    "                           default = 1\n"
    "    -o cache.entry=INT     File name lookup cache timeout in seconds.\n"
    "                           default = 1\n"
    "    -o cache.negative_entry=INT\n"
    "                           Negative file name lookup cache timeout in\n"
    "                           seconds. default = 0\n"
    "    -o use_ino             Have mergerfs generate inode values rather than\n"
    "                           autogenerated by libfuse. Suggested.\n"
    "    -o inodecalc=passthrough|path-hash|devino-hash|hybrid-hash\n"
    "                           Selects the inode calculation algorithm.\n"
    "                           default = hybrid-hash\n"
    "    -o minfreespace=INT    Minimum free space needed for certain policies.\n"
    "                           default = 4G\n"
    "    -o moveonenospc=BOOL   Try to move file to another drive when ENOSPC\n"
    "                           on write. default = false\n"
    "    -o dropcacheonclose=BOOL\n"
    "                           When a file is closed suggest to OS it drop\n"
    "                           the file's cache. This is useful when using\n"
    "                           'cache.files'. default = false\n"
    "    -o symlinkify=BOOL     Read-only files, after a timeout, will be turned\n"
    "                           into symlinks. Read docs for limitations and\n"
    "                           possible issues. default = false\n"
    "    -o symlinkify_timeout=INT\n"
    "                           Timeout in seconds before files turn to symlinks.\n"
    "                           default = 3600\n"
    "    -o nullrw=BOOL         Disables reads and writes. For benchmarking.\n"
    "                           default = false\n"
    "    -o ignorepponrename=BOOL\n"
    "                           Ignore path preserving when performing renames\n"
    "                           and links. default = false\n"
    "    -o link_cow=BOOL       Delink/clone file on open to simulate CoW.\n"
    "                           default = false\n"
    "    -o nfsopenhack=off|git|all\n"
    "                           A workaround for exporting mergerfs over NFS\n"
    "                           where there are issues with creating files for\n"
    "                           write while setting the mode to read-only.\n"
    "                           default = off\n"
    "    -o security_capability=BOOL\n"
    "                           When disabled return ENOATTR when the xattr\n"
    "                           security.capability is queried. default = true\n"
    "    -o xattr=passthrough|noattr|nosys\n"
    "                           Runtime control of xattrs. By default xattr\n"
    "                           requests will pass through to the underlying\n"
    "                           filesystems. notattr will short circuit as if\n"
    "                           nothing exists. nosys will respond as if not\n"
    "                           supported or disabled. default = passthrough\n"
    "    -o statfs=base|full    When set to 'base' statfs will use all branches\n"
    "                           when performing statfs calculations. 'full' will\n"
    "                           only include branches on which that path is\n"
    "                           available. default = base\n"
    "    -o statfs_ignore=none|ro|nc\n"
    "                           'ro' will cause statfs calculations to ignore\n"
    "                           available space for branches mounted or tagged\n"
    "                           as 'read only' or 'no create'. 'nc' will ignore\n"
    "                           available space for branches tagged as\n"
    "                           'no create'. default = none\n"
    "    -o posix_acl=BOOL      Enable POSIX ACL support. default = false\n"
    "    -o async_read=BOOL     If disabled or unavailable the kernel will\n"
    "                           ensure there is at most one pending read \n"
    "                           request per file and will attempt to order\n"
    "                           requests by offset. default = true\n"
            << std::endl;
}

static
int
option_processor(void       *data_,
                 const char *arg_,
                 int         key_,
                 fuse_args  *outargs_)
{
  Config::Write cfg;
  Config::ErrVec *errs = (Config::ErrVec*)data_;

  switch(key_)
    {
    case FUSE_OPT_KEY_OPT:
      return process_opt(cfg,errs,arg_);

    case FUSE_OPT_KEY_NONOPT:
      if(cfg->branches->empty())
        return process_branches(cfg,errs,arg_);
      else
        return process_mount(cfg,errs,arg_);

    case MERGERFS_OPT_HELP:
      usage();
      exit(0);

    case MERGERFS_OPT_VERSION:
      std::cout << "mergerfs version: "
                << (MERGERFS_VERSION[0] ? MERGERFS_VERSION : "unknown")
                << std::endl;
      exit(0);

    default:
      break;
    }

  return 0;
}

namespace options
{
  void
  parse(fuse_args      *args_,
        Config::ErrVec *errs_)
  {
    Config::Write cfg;
    const struct fuse_opt opts[] =
      {
        FUSE_OPT_KEY("-h",MERGERFS_OPT_HELP),
        FUSE_OPT_KEY("--help",MERGERFS_OPT_HELP),
        FUSE_OPT_KEY("-v",MERGERFS_OPT_VERSION),
        FUSE_OPT_KEY("-V",MERGERFS_OPT_VERSION),
        FUSE_OPT_KEY("--version",MERGERFS_OPT_VERSION),
        {NULL,-1U,0}
      };

    fuse_opt_parse(args_,
                   errs_,
                   opts,
                   ::option_processor);

    if(cfg->branches->empty())
      errs_->push_back({0,"branches not set"});
    if(cfg->mount->empty())
      errs_->push_back({0,"mountpoint not set"});

    set_default_options(args_);
    set_fsname(cfg,args_);
    set_subtype(args_);
    set_threads(cfg,args_);
  }
}
