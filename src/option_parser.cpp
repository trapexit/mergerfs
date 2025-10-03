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
#include "fuse_config.hpp"

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
    MERGERFS_OPT_HELP,
    MERGERFS_OPT_VERSION
  };

struct OptParseState
{
  std::vector<std::string> opts;
  std::vector<std::string> branches;
  Config::ErrVec *errs;
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
_set_fuse_threads(Config &cfg_)
{
  fuse_config_set_read_thread_count(cfg_.fuse_read_thread_count);
  fuse_config_set_process_thread_count(cfg_.fuse_process_thread_count);
  fuse_config_set_process_thread_queue_depth(cfg_.fuse_process_thread_queue_depth);
  fuse_config_set_pin_threads(cfg_.fuse_pin_threads);
}

static
void
_set_fsname(Config    &cfg_,
            fuse_args *args_)
{
  if(cfg_.fsname->empty())
    {
      std::vector<std::string> paths;

      cfg_.branches->to_paths(paths);

      if(paths.size() > 0)
        cfg_.fsname = str::remove_common_prefix_and_join(paths,':');
    }

  ::_set_kv_option("fsname",cfg_.fsname,args_);
}

static
void
_set_subtype(fuse_args *args_)
{
  ::_set_kv_option("subtype","mergerfs",args_);
}

static
void
_set_default_options(fuse_args *args_,
                     Config    &cfg_)
{
  if(cfg_.kernel_permissions_check)
    ::_set_option("default_permissions",args_);

  if(geteuid() == 0)
    ::_set_option("allow_other",args_);
  else
    SysLog::notice("not auto setting allow_other since not running as root");
}

static
bool
_should_ignore(const std::string &key_)
{
  constexpr const std::array<std::string_view,13> ignored_keys =
    {
      "atomic_o_trunc",
      "big_writes",
      "cache.open",
      "defaults",
      "hard_remove",
      "no_splice_move",
      "no_splice_read",
      "no_splice_write",
      "nonempty",
      "splice_move",
      "splice_read",
      "splice_write",
      "use_ino",
    };

  for(const auto &key : ignored_keys)
    {
      if(key == key_)
        return true;
    }

  return false;
}

static
int
_parse_and_process_kv_arg(Config            &cfg_,
                          Config::ErrVec    *errs_,
                          const std::string &key_,
                          const std::string &val_)
{
  int rv;
  std::string key(key_);
  std::string val(val_);

  rv = 0;
  if(key == "config")
    return ((cfg_.from_file(val_,errs_) < 0) ? 1 : 0);
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
  ef(::_should_ignore(key_))
    return 0;

  if(cfg_.has_key(key) == false)
    return 1;

  rv = cfg_.set_raw(key,val);
  if(rv)
    errs_->push_back({rv,key+'='+val});

  return 0;
}

static
int
_process_opt(Config            &cfg_,
             Config::ErrVec    *errs_,
             const std::string &arg_)
{
  std::string key;
  std::string val;

  str::splitkv(arg_,'=',&key,&val);
  key = str::trim(key);
  val = str::trim(val);

  return ::_parse_and_process_kv_arg(cfg_,errs_,key,val);
}

static
int
_process_branches(Config        &cfg_,
                  Config::ErrVec *errs_,
                  const char     *arg_)
{
  int rv;
  std::string arg;

  arg = arg_;
  rv = cfg_.set_raw("branches",arg);
  if(rv)
    errs_->push_back({rv,"branches="+arg});

  return 0;
}

static
int
_process_mount(Config        &cfg_,
               Config::ErrVec *errs_,
               const char     *arg_)
{
  int rv;
  std::string arg;

  arg = arg_;
  rv = cfg_.set_raw("mount",arg);
  if(rv)
    errs_->push_back({rv,"mount="+arg});

  return 1;
}

static
void
_postprocess_passthrough(Config &cfg_)
{
  if(cfg_.passthrough == Passthrough::ENUM::OFF)
    return;

  if(cfg_.cache_files == CacheFiles::ENUM::OFF)
    {
      SysLog::warning("'cache.files' can not be 'off' when using 'passthrough'."
                      " Setting 'cache.files=full'");
      cfg_.cache_files = CacheFiles::ENUM::FULL;
    }

  if(cfg_.writeback_cache == true)
    {
      SysLog::warning("'cache.writeback' can not be enabled when using 'passthrough'."
                      " Setting 'cache.writeback=false'");
      cfg_.writeback_cache = false;
    }

  if(cfg_.moveonenospc.enabled == true)
    {
      SysLog::warning("`moveonenospc` will not function when `passthrough` is enabled");
    }
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
int
_option_processor(void       *data_,
                  const char *arg_,
                  int         key_,
                  fuse_args  *outargs_)
{
  OptParseState &state = *(OptParseState*)data_;

  switch(key_)
    {
    case FUSE_OPT_KEY_OPT:
      return ::_process_opt(cfg,state.errs,arg_);

    case FUSE_OPT_KEY_NONOPT:
      state.branches.push_back(arg_);
      return 0;

    case MERGERFS_OPT_HELP:
      ::_usage();
      exit(0);

    case MERGERFS_OPT_VERSION:
      fmt::print("mergerfs v{}\n\n"
                 "https://github.com/trapexit/mergerfs\n"
                 "https://trapexit.github.io/mergerfs\n"
                 "https://github.com/trapexit/support\n\n"
                 "ISC License (ISC)\n\n"
                 "Copyright 2025, Antonio SJ Musumeci <trapexit@spawn.link>\n\n"
                 "Permission to use, copy, modify, and/or distribute this software for\n"
                 "any purpose with or without fee is hereby granted, provided that the\n"
                 "above copyright notice and this permission notice appear in all\n"
                 "copies.\n\n"
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
      exit(0);

    default:
      break;
    }

  return 0;
}

static
void
_check_for_mount_loop(Config         &cfg_,
                      Config::ErrVec *errs_)
{
  fs::path mount;
  std::vector<fs::path> branches;
  std::error_code ec;

  mount    = *cfg_.mountpoint;
  branches = cfg_.branches->to_paths();
  for(const auto &branch : branches)
    {
      if(std::filesystem::equivalent(branch,mount,ec))
        {
          std::string errstr;

          errstr = fmt::format("branches can not include the mountpoint: {}",
                               branch.string());
          errs_->push_back({0,errstr});
        }
    }
}

static
void
_print_warnings(Config &cfg_)
{
  if(cfg_.passthrough != Passthrough::ENUM::OFF)
    {
      if(cfg_.cache_files == CacheFiles::ENUM::OFF)
        {
          SysLog::warning("'cache.files' can not be 'off' when using 'passthrough'."
                          " Setting 'cache.files=auto-full'");
          cfg_.cache_files = CacheFiles::ENUM::AUTO_FULL;
        }

      if(cfg_.writeback_cache == true)
        {
          SysLog::warning("'cache.writeback' can not be enabled when using 'passthrough'."
                          " Setting 'cache.writeback=false'");
          cfg_.writeback_cache = false;
        }

      if(cfg_.moveonenospc.enabled == true)
        {
          SysLog::warning("`moveonenospc` will not function when `passthrough` is enabled");
        }
    }
}

static
void
_cleanup_options(Config &cfg_)
{
  if(!cfg_.symlinkify)
    cfg_.symlinkify_timeout = -1;
}

namespace options
{
  void
  parse(fuse_args      *args_,
        Config::ErrVec *errs_)
  {
    const struct fuse_opt opts[] =
      {
        FUSE_OPT_KEY("-h",MERGERFS_OPT_HELP),
        FUSE_OPT_KEY("--help",MERGERFS_OPT_HELP),
        FUSE_OPT_KEY("-v",MERGERFS_OPT_VERSION),
        FUSE_OPT_KEY("-V",MERGERFS_OPT_VERSION),
        FUSE_OPT_KEY("--version",MERGERFS_OPT_VERSION),
        {NULL,-1U,0}
      };
    OptParseState state{.errs=errs_};

    fuse_opt_parse(args_,
                   &state,
                   opts,
                   ::_option_processor);

    if(state.branches.size() < 2)
      errs_->push_back({0,"mountpoint not set"});
    if(state.branches.size() < 1)
      errs_->push_back({0,"branches not set"});
    if(!errs_->empty())
      return;

    _process_mount(cfg,
                   errs_,
                   state.branches.back().c_str());

    state.branches.pop_back();
    _process_branches(cfg,
                      errs_,
                      str::join(state.branches,':').c_str());

    if(cfg.branches->empty())
      errs_->push_back({0,"branches not set"});
    if(cfg.mountpoint->empty())
      errs_->push_back({0,"mountpoint not set"});
    ::_check_for_mount_loop(cfg,errs_);
    if(!errs_->empty())
      return;

    fuse_opt_add_arg(args_,cfg.mountpoint.c_str());

    ::_postprocess_passthrough(cfg);
    ::_set_default_options(args_,cfg);
    ::_set_fsname(cfg,args_);
    ::_set_subtype(args_);
    ::_set_fuse_threads(cfg);
    ::_print_warnings(cfg);
    ::_cleanup_options(cfg);

    cfg.finish_initializing();
  }
}
