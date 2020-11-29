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

#include <fuse.h>

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

struct Data
{
  Config         *config;
  vector<string> *errs;
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

  static
  void
  read_config(Data         *data_,
              std::istream &is_)
  {
    int rv;
    std::string key;
    std::string val;
    std::string line;

    rv = 0;
    while(std::getline(is_,line,'\n'))
      {
        line = str::trim(line);
        if(!line.empty() && (line[0] == '#'))
          continue;

        str::splitkv(line,'=',&key,&val);
        key = str::trim(key);
        val = str::trim(val);

        if(key.empty() || val.empty())
          continue;

        rv = data_->config->set_raw(key,val);
        switch(rv)
          {
          case -EINVAL:
            data_->errs->push_back("invalid argument - " + line);
            break;
          case -ENOATTR:
            data_->errs->push_back("unknown option - " + line);
            break;
          default:
            break;
          }
      }
  }

  static
  void
  read_config(Data              *data_,
              const std::string &filepath_)
  {
    std::ifstream is;

    is.open(filepath_);
    if(is.bad())
      {
        data_->errs->push_back("unable to open config - " + filepath_);
      }
    else
      {
        l::read_config(data_,is);
        if(!is.eof())
          data_->errs->push_back("failure reading config file");
        is.close();
      }
  }
}

static
void
set_option(fuse_args         *args,
           const std::string &option_)
{

  fuse_opt_add_arg(args,"-o");
  fuse_opt_add_arg(args,option_.c_str());
}

static
void
set_kv_option(fuse_args         *args,
              const std::string &key,
              const std::string &value)
{
  std::string option;

  option = key + '=' + value;

  set_option(args,option);
}

static
void
set_threads(fuse_args *args_,
            Config    *config_)
{
  int threads;

  threads = l::calculate_thread_count(config_->threads);

  config_->threads = threads;

  set_kv_option(args_,"threads",config_->threads.to_string());
}

static
void
set_fsname(fuse_args *args_,
           Config    *config_)
{
  if(config_->fsname->empty())
    {
      vector<string> branches;

      config_->branches.to_paths(branches);

      if(branches.size() > 0)
        config_->fsname = str::remove_common_prefix_and_join(branches,':');
    }

  set_kv_option(args_,"fsname",config_->fsname);
}

static
void
set_subtype(fuse_args *args)
{
  set_kv_option(args,"subtype","mergerfs");
}

static
void
set_default_options(fuse_args *args)
{
  set_option(args,"default_permissions");
}

static
int
parse_and_process_kv_arg(Data              *data_,
                         const std::string &key_,
                         const std::string &val_)
{
  int rv;
  std::string key(key_);
  std::string val(val_);

  rv = 0;
  if(key == "config")
    return (l::read_config(data_,val_),0);
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

  if(data_->config->has_key(key) == false)
    return 1;

  rv = data_->config->set_raw(key,val);
  if(rv)
    data_->errs->push_back("invalid argument - " + key_ + '=' + val_);

  return 0;
}

static
int
process_opt(Data              *data_,
            const std::string &arg_)
{
  std::string key;
  std::string val;

  str::splitkv(arg_,'=',&key,&val);

  return parse_and_process_kv_arg(data_,key,val);
}

static
int
process_branches(Data       *data_,
                 const char *arg_)
{
  int rv;
  string arg;

  arg = arg_;
  rv = data_->config->set_raw("branches",arg);
  if(rv)
    data_->errs->push_back("unable to parse 'branches' - " + arg);

  return 0;
}

static
int
process_mount(Data       *data_,
              const char *arg_)
{
  int rv;
  string arg;

  arg = arg_;
  rv = data_->config->set_raw("mount",arg);
  if(rv)
    data_->errs->push_back("unable to set 'mount' - " + arg);

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
  Data *data = (Data*)data_;

  switch(key_)
    {
    case FUSE_OPT_KEY_OPT:
      return process_opt(data,arg_);

    case FUSE_OPT_KEY_NONOPT:
      if(data->config->branches.vec.empty())
        return process_branches(data,arg_);
      else
        return process_mount(data,arg_);

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
  parse(fuse_args                *args_,
        Config                   *config_,
        std::vector<std::string> *errs_)
  {
    Data data;
    const struct fuse_opt opts[] =
      {
        FUSE_OPT_KEY("-h",MERGERFS_OPT_HELP),
        FUSE_OPT_KEY("--help",MERGERFS_OPT_HELP),
        FUSE_OPT_KEY("-v",MERGERFS_OPT_VERSION),
        FUSE_OPT_KEY("-V",MERGERFS_OPT_VERSION),
        FUSE_OPT_KEY("--version",MERGERFS_OPT_VERSION),
        {NULL,-1U,0}
      };

    data.config = config_;
    data.errs   = errs_;
    fuse_opt_parse(args_,
                   &data,
                   opts,
                   ::option_processor);

    if(config_->branches.vec.empty())
      errs_->push_back("branches not set");
    if(config_->mount->empty())
      errs_->push_back("mountpoint not set");

    set_default_options(args_);
    set_fsname(args_,config_);
    set_subtype(args_);
    set_threads(args_,config_);
  }
}
