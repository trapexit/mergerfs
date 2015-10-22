/*
   The MIT License (MIT)

   Copyright (c) 2014 Antonio SJ Musumeci <trapexit@spawn.link>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
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
parse_and_process_minfreespace(const std::string &value,
                               size_t            &minfreespace)
{
  int rv;

  rv = num::to_size_t(value,minfreespace);
  if(rv == -1)
    return 1;

  return 0;
}

static
int
parse_and_process_moveonenospc(const std::string &value,
                               bool              &moveonenospc)
{
  if(value == "false")
    moveonenospc = false;
  else if(value == "true")
    moveonenospc = true;
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
    {
      set_default_options(*outargs);
      return 0;
    }

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
        rv = parse_and_process_minfreespace(value,config.minfreespace);
      else if(key == "moveonenospc")
        rv = parse_and_process_moveonenospc(value,config.moveonenospc);
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
    "    -o defaults            default FUSE options which seem to provide the\n"
    "                           best performance: atomic_o_trunc, auto_cache,\n"
    "                           big_writes, default_permissions, splice_read,\n"
    "                           splice_write, splice_move\n"
    "    -o direct_io           bypass additional caching, increases write\n"
    "                           speeds at the cost of reads\n"
    "    -o minfreespace=<int>  minimum free space needed for certain policies:\n"
    "                           default 4G\n"
    "    -o moveonenospc=<bool> try to move file to another drive when ENOSPC\n"
    "                           on write: default false\n"
    "    -o func.<f>=<p>        set function <f> to policy <p>\n"
    "    -o category.<c>=<p>    set functions in category <c> to <p>\n"
            << std::endl;
}

static
void
version(void)
{
  std::cout << "mergerfs version: " << MERGERFS_VERSION
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
      version();
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
