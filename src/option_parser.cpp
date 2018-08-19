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

#include <fuse.h>

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>

#include "config.hpp"
#include "num.hpp"
#include "policy.hpp"
#include "str.hpp"
#include "version.hpp"

using std::string;
using std::vector;
using namespace mergerfs;

enum
  {
    MERGERFS_OPT_HELP,
    MERGERFS_OPT_VERSION
  };

static
void
set_option(fuse_args         &args,
           const std::string &option_)
{
  string option;

  option = "-o" + option_;

  fuse_opt_insert_arg(&args,1,option.c_str());
}

static
void
set_kv_option(fuse_args         &args,
              const std::string &key,
              const std::string &value)
{
  std::string option;

  option = key + '=' + value;

  set_option(args,option);
}

static
void
set_fsname(fuse_args            &args,
           const vector<string> &srcmounts)
{
  if(srcmounts.size() > 0)
    {
      std::string fsname;

      fsname = str::remove_common_prefix_and_join(srcmounts,':');

      set_kv_option(args,"fsname",fsname);
    }
}

static
void
set_subtype(fuse_args &args)
{
  set_kv_option(args,"subtype","mergerfs");
}

static
void
set_default_options(fuse_args &args)
{
  set_option(args,"atomic_o_trunc");
  set_option(args,"auto_cache");
  set_option(args,"big_writes");
  set_option(args,"default_permissions");
  set_option(args,"splice_move");
  set_option(args,"splice_read");
  set_option(args,"splice_write");
}

static
int
parse_and_process(const std::string &value,
                  uint64_t          &minfreespace)
{
  int rv;

  rv = num::to_uint64_t(value,minfreespace);
  if(rv == -1)
    return 1;

  return 0;
}

static
int
parse_and_process(const std::string &value,
                  time_t            &time)
{
  int rv;

  rv = num::to_time_t(value,time);
  if(rv == -1)
    return 1;

  return 0;
}

static
int
parse_and_process(const std::string &value,
                  bool              &boolean)
{
  if(value == "false")
    boolean = false;
  else if(value == "true")
    boolean = true;
  else
    return 1;

  return 0;
}

static
int
parse_and_process_arg(Config            &config,
                      const std::string &arg,
                      fuse_args         *outargs)
{
  if(arg == "defaults")
    return (set_default_options(*outargs),0);
  else if(arg == "direct_io")
    return (config.direct_io=true,1);

  return 1;
}

static
int
parse_and_process_kv_arg(Config            &config,
                         const std::string &key,
                         const std::string &value)
{
  int rv = -1;
  std::vector<std::string> keypart;

  str::split(keypart,key,'.');
  if(keypart.size() == 2)
    {
      if(keypart[0] == "func")
        rv = config.set_func_policy(keypart[1],value);
      else if(keypart[0] == "category")
        rv = config.set_category_policy(keypart[1],value);
    }
  else
    {
      if(key == "minfreespace")
        rv = parse_and_process(value,config.minfreespace);
      else if(key == "moveonenospc")
        rv = parse_and_process(value,config.moveonenospc);
      else if(key == "dropcacheonclose")
        rv = parse_and_process(value,config.dropcacheonclose);
      else if(key == "symlinkify")
        rv = parse_and_process(value,config.symlinkify);
      else if(key == "symlinkify_timeout")
        rv = parse_and_process(value,config.symlinkify_timeout);
      else if(key == "nullrw")
        rv = parse_and_process(value,config.nullrw);
      else if(key == "ignorepponrename")
        rv = parse_and_process(value,config.ignorepponrename);
    }

  if(rv == -1)
    rv = 1;

  return rv;
}

static
int
process_opt(Config            &config,
            const std::string &arg,
            fuse_args         *outargs)
{
  int rv;
  std::vector<std::string> argvalue;

  str::split(argvalue,arg,'=');
  switch(argvalue.size())
    {
    case 1:
      rv = parse_and_process_arg(config,argvalue[0],outargs);
      break;

    case 2:
      rv = parse_and_process_kv_arg(config,argvalue[0],argvalue[1]);
      break;

    default:
      rv = 1;
      break;
    };

  return rv;
}

static
int
process_srcmounts(const char *arg,
                  Config     &config)
{
  vector<string> paths;

  str::split(paths,arg,':');

  fs::glob(paths,config.srcmounts);

  fs::realpathize(config.srcmounts);

  return 0;
}

static
int
process_destmounts(const char *arg,
                   Config     &config)
{
  config.destmount = arg;

  return 1;
}

static
void
usage(void)
{
  std::cout <<
    "Usage: mergerfs [options] <srcpaths> <destpath>\n"
    "\n"
    "    -o [opt,...]           mount options\n"
    "    -h --help              print help\n"
    "    -v --version           print version\n"
    "\n"
    "mergerfs options:\n"
    "    <srcpaths>             ':' delimited list of directories. Supports\n"
    "                           shell globbing (must be escaped in shell)\n"
    "    -o defaults            Default FUSE options which seem to provide the\n"
    "                           best performance: atomic_o_trunc, auto_cache,\n"
    "                           big_writes, default_permissions, splice_read,\n"
    "                           splice_write, splice_move\n"
    "    -o func.<f>=<p>        Set function <f> to policy <p>\n"
    "    -o category.<c>=<p>    Set functions in category <c> to <p>\n"
    "    -o direct_io           Bypass additional caching, increases write\n"
    "                           speeds at the cost of reads. Please read docs\n"
    "                           for more details as there are tradeoffs.\n"
    "    -o use_ino             Have mergerfs generate inode values rather than\n"
    "                           autogenerated by libfuse. Suggested.\n"
    "    -o minfreespace=<int>  minimum free space needed for certain policies.\n"
    "                           default=4G\n"
    "    -o moveonenospc=<bool> Try to move file to another drive when ENOSPC\n"
    "                           on write. default=false\n"
    "    -o dropcacheonclose=<bool>\n"
    "                           When a file is closed suggest to OS it drop\n"
    "                           the file's cache. This is useful when direct_io\n"
    "                           is disabled. default=false\n"
    "    -o symlinkify=<bool>   Read-only files, after a timeout, will be turned\n"
    "                           into symlinks. Read docs for limitations and\n"
    "                           possible issues. default=false\n"
    "    -o symlinkify_timeout=<int>\n"
    "                           timeout in seconds before will turn to symlinks.\n"
    "                           default=3600\n"
    "    -o nullrw=<bool>       Disables reads and writes. For benchmarking.\n"
    "    -o ignorepponrename=<bool>\n"
    "                           Ignore path preserving when performing renames\n"
    "                           and links. default = false\n"
            << std::endl;
}

static
int
option_processor(void       *data,
                 const char *arg,
                 int         key,
                 fuse_args  *outargs)
{
  int     rv     = 0;
  Config &config = *(Config*)data;

  switch(key)
    {
    case FUSE_OPT_KEY_OPT:
      rv = process_opt(config,arg,outargs);
      break;

    case FUSE_OPT_KEY_NONOPT:
      rv = config.srcmounts.empty() ?
        process_srcmounts(arg,config) :
        process_destmounts(arg,config);
      break;

    case MERGERFS_OPT_HELP:
      usage();
      close(2);
      dup(1);
      fuse_opt_add_arg(outargs,"-ho");
      break;

    case MERGERFS_OPT_VERSION:
      std::cout << "mergerfs version: "
                << (MERGERFS_VERSION[0] ? MERGERFS_VERSION : "unknown")
                << std::endl;
      fuse_opt_add_arg(outargs,"--version");
      break;

    default:
      break;
    }

  return rv;
}

namespace mergerfs
{
  namespace options
  {
    void
    parse(fuse_args &args,
          Config    &config)
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


      fuse_opt_parse(&args,
                     &config,
                     opts,
                     ::option_processor);

      set_fsname(args,config.srcmounts);
      set_subtype(args);
    }
  }
}
