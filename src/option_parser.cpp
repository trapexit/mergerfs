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

#define FMT_HEADER_ONLY

#include "config.hpp"
#include "ef.hpp"
#include "errno.hpp"
#include "fmt/core.h"
#include "fs_glob.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "hw_cpu.hpp"
#include "num.hpp"
#include "policy.hpp"
#include "str.hpp"
#include "syslog.hpp"
#include "version.hpp"

#include "fuse.h"

#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum
  {
    MERGERFS_OPT_DEBUG,
    MERGERFS_OPT_HELP,
    MERGERFS_OPT_VERSION
  };

enum
  {
    OPT_DISCARD = 0,
    OPT_KEEP    = 1
  };

static
void
_set_option(const std::string &option_,
            fuse_args         *args_)
{

  fuse_opt_add_arg(args_,"-o");
  fuse_opt_add_arg(args_,option_.c_str());
}

static
void
_set_kv_option(const std::string &key_,
               const std::string &val_,
               fuse_args         *args_)
{
  std::string option;

  option = key_ + '=' + val_;

  ::_set_option(option,args_);
}

static
void
_set_fsname(fuse_args *args_)
{
  if(cfg.fsname->empty())
    {
      std::vector<std::string> paths;

      cfg.branches->to_paths(paths);

      if(paths.size() > 0)
        cfg.fsname = str::remove_common_prefix_and_join(paths,':');
    }

  ::_set_kv_option("fsname",cfg.fsname,args_);
}

static
void
_set_subtype(fuse_args *args_)
{
  ::_set_kv_option("subtype","mergerfs",args_);
}

static
void
_set_default_options(fuse_args *args_)
{
  if(cfg.kernel_permissions_check)
    ::_set_option("default_permissions",args_);

  if(geteuid() == 0)
    ::_set_option("allow_other",args_);
  else
    SysLog::notice("not auto setting allow_other since not running as root");
}

static
int
_process_opt(const std::string &arg_)
{
  int rv;

  rv = cfg.set(arg_);
  if(rv == -ENOATTR)
    return OPT_KEEP;
  if(rv < 0)
    cfg.errs.push_back({-rv,arg_});

  return OPT_DISCARD;
}

static
void
_usage(void)
{
  fmt::print("Usage: mergerfs [options] <branches> <mountpoint>\n"
             "\n"
             "Visit https://trapexit.github.io/mergerfs for options.\n\n");
}

static
void
_version(void)
{
  fmt::print("mergerfs v{}\n\n"
             "https://github.com/trapexit/mergerfs\n"
             "https://trapexit.github.io/mergerfs\n"
             "https://github.com/trapexit/support\n"
             "\n"
             "ISC License (ISC)\n"
             "\n"
             "Copyright 2025, Antonio SJ Musumeci <trapexit@spawn.link>\n"
             "\n"
             "Permission to use, copy, modify, and/or distribute this software for\n"
             "any purpose with or without fee is hereby granted, provided that the\n"
             "above copyright notice and this permission notice appear in all\n"
             "copies.\n"
             "\n"
             "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL\n"
             "WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED\n"
             "WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE\n"
             "AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL\n"
             "DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR\n"
             "PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER\n"
             "TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR\n"
             "PERFORMANCE OF THIS SOFTWARE.\n\n"
             ,
             (MERGERFS_VERSION[0] ? MERGERFS_VERSION : "unknown"));
}

static
int
_option_processor(void       *data_,
                  const char *arg_,
                  int         key_,
                  fuse_args  *outargs_)
{
  std::string &prev_nonopt =
    *reinterpret_cast<std::string*>(data_);

  switch(key_)
    {
    case FUSE_OPT_KEY_OPT:
      return ::_process_opt(arg_);

    case FUSE_OPT_KEY_NONOPT:
      {
        if(prev_nonopt.empty())
          {
            prev_nonopt = arg_;
          }
        else
          {
            int rv;
            rv = cfg.branches.from_string("+>" + prev_nonopt);
            if(rv < 0)
              cfg.errs.push_back({-rv,prev_nonopt});
            prev_nonopt = arg_;
          }
      }
      return OPT_DISCARD;

    case MERGERFS_OPT_DEBUG:
      fuse_cfg.debug = true;
      return OPT_DISCARD;

    case MERGERFS_OPT_HELP:
      ::_usage();
      exit(0);

    case MERGERFS_OPT_VERSION:
      ::_version();
      exit(0);

    default:
      break;
    }

  return 0;
}

static
void
_check_for_mount_loop()
{
  fs::path mount;
  std::vector<fs::path> branches;
  std::error_code ec;

  mount    = cfg.mountpoint;
  branches = cfg.branches->to_paths();
  for(const auto &branch : branches)
    {
      if(std::filesystem::equivalent(branch,mount,ec))
        {
          std::string errstr;

          errstr = fmt::format("branches can not include the mountpoint: {}",
                               branch.string());
          cfg.errs.push_back({0,errstr});
        }
    }
}

static
void
_postprocess_and_print_warnings()
{
  if(fuse_cfg.uid != FUSE_CFG_INVALID_ID)
    SysLog::warning("overwriting 'uid' is untested and unsupported,"
                    " use at your own risk");
  if(fuse_cfg.gid != FUSE_CFG_INVALID_ID)
    SysLog::warning("overwriting 'gid' is untested and unsupported,"
                    " use at your own risk");
  if(fuse_cfg.umask != FUSE_CFG_INVALID_UMASK)
    SysLog::warning("overwriting 'umask' is untested and unsupported,"
                    " use at your own risk");

  if(cfg.passthrough_io != PassthroughIO::ENUM::OFF)
    {
      if(cfg.cache_files == CacheFiles::ENUM::OFF)
        {
          SysLog::warning("'cache.files' can not be 'off' when using 'passthrough'."
                          " Setting 'cache.files=auto-full'");
          cfg.cache_files = CacheFiles::ENUM::AUTO_FULL;
        }

      if(cfg.cache_writeback == true)
        {
          SysLog::warning("'cache.writeback' can not be enabled when using 'passthrough'."
                          " Setting 'cache.writeback=false'");
          cfg.cache_writeback = false;
        }

      if(cfg.moveonenospc.enabled == true)
        {
          SysLog::warning("'moveonenospc' will not function when 'passthrough' is enabled");
        }
    }
}

static
void
_cleanup_options()
{
  if(!cfg.symlinkify)
    cfg.symlinkify_timeout = -1;
}

namespace options
{
  void
  parse(fuse_args *args_)

  {
    std::string prev_nonopt;
    const struct fuse_opt opts[] =
      {
        FUSE_OPT_KEY("-d",MERGERFS_OPT_DEBUG),
        FUSE_OPT_KEY("-h",MERGERFS_OPT_HELP),
        FUSE_OPT_KEY("--help",MERGERFS_OPT_HELP),
        FUSE_OPT_KEY("-v",MERGERFS_OPT_VERSION),
        FUSE_OPT_KEY("-V",MERGERFS_OPT_VERSION),
        FUSE_OPT_KEY("--version",MERGERFS_OPT_VERSION),
        {NULL,-1U,0}
      };

    fuse_opt_parse(args_,
                   &prev_nonopt,
                   opts,
                   ::_option_processor);

    if(!prev_nonopt.empty())
      {
        if(cfg.mountpoint.empty())
          {
            cfg.mountpoint = prev_nonopt;
          }
        else
          {
            int rv;
            rv = cfg.branches.from_string("+>" + prev_nonopt);
            if(rv < 0)
              cfg.errs.push_back({-rv,prev_nonopt});
          }
      }

    if(cfg.branches->empty())
      cfg.errs.push_back({ENOENT,"branches not set"});
    if(cfg.mountpoint.empty())
      cfg.errs.push_back({ENOENT,"mountpoint not set"});

    if(!cfg.errs.empty())
      return;

    fuse_opt_add_arg(args_,cfg.mountpoint.c_str());

    ::_postprocess_and_print_warnings();
    ::_check_for_mount_loop();
    ::_set_default_options(args_);
    ::_set_fsname(args_);
    ::_set_subtype(args_);
    ::_cleanup_options();

    cfg.finish_initializing();
  }
}
