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

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include "str.hpp"
#include "config.hpp"
#include "policy.hpp"

using std::string;
using std::vector;
using namespace mergerfs;

static
void
set_option(struct fuse_args  &args,
           const std::string &option_)
{
  string option;

  option = "-o" + option_;

  fuse_opt_insert_arg(&args,1,option.c_str());
}

static
void
set_kv_option(struct fuse_args  &args,
              const std::string &key,
              const std::string &value)
{
  std::string option;

  option = key + '=' + value;

  set_option(args,option);
}

static
void
set_fsname(struct fuse_args     &args,
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
set_subtype(struct fuse_args &args)
{
  set_kv_option(args,"subtype","mergerfs");
}

static
void
set_default_options(struct fuse_args &args)
{
  set_option(args,"big_writes");
  set_option(args,"splice_read");
  set_option(args,"splice_write");
  set_option(args,"splice_move");
  set_option(args,"auto_cache");
  set_option(args,"atomic_o_trunc");
}

static
int
parse_and_process_arg(config::Config    &config,
                      const std::string &arg,
                      struct fuse_args  *outargs)
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
parse_and_process_kv_arg(config::Config    &config,
                         const std::string &key,
                         const std::string &value)
{
  int rv;
  std::vector<std::string> keypart;

  str::split(keypart,key,'.');
  if(keypart.size() != 2)
    return 1;

  if(keypart[0] == "func")
    rv = config.set_func_policy(keypart[1],value);
  else if(keypart[0] == "category")
    rv = config.set_category_policy(keypart[1],value);
  else
    rv = -1;

  if(rv == -1)
    rv = 1;

  return rv;
}

static
int
process_opt(config::Config    &config,
            const std::string &arg,
            struct fuse_args  *outargs)
{
  int rv;
  std::vector<std::string> argvalue;

  rv = 1;
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
process_srcmounts(const char     *arg,
                  config::Config &config)
{
  vector<string> paths;

  str::split(paths,arg,':');

  fs::glob(paths,config.srcmounts);

  return 0;
}

static
int
process_destmounts(const char     *arg,
                   config::Config &config)
{
  config.destmount = arg;

  return 1;
}

static
int
option_processor(void             *data,
                 const char       *arg,
                 int               key,
                 struct fuse_args *outargs)
{
  int rv = 0;
  config::Config &config = *(config::Config*)data;

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
    parse(struct fuse_args &args,
          config::Config   &config)
    {

      fuse_opt_parse(&args,
                     &config,
                     NULL,
                     ::option_processor);

      set_fsname(args,config.srcmounts);
      set_subtype(args);
    }
  }
}
