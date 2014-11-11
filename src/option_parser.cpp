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
int
process_opt(config::Config    &config,
            const std::string &arg)
{
  int rv = 0;
  std::vector<std::string> argvalue;

  str::split(argvalue,arg,'=');
  switch(argvalue.size())
    {
    case 2:
      if(argvalue[0] == "create")
        config.create = Policy::find(argvalue[1]);
      else if(argvalue[0] == "search")
        config.search = Policy::find(argvalue[1]);
      else if(argvalue[0] == "action")
        config.action = Policy::find(argvalue[1]);
      else
        rv = 1;
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
      rv = process_opt(config,arg);
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

static
void
set_fsname(struct fuse_args     &args,
           const config::Config &config)
{
  if(config.srcmounts.size() > 0)
    {
      std::string fsname;

      fsname = "-ofsname=" + str::join(config.srcmounts,':');

      fuse_opt_insert_arg(&args,1,fsname.c_str());
    }
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

      set_fsname(args,config);
    }
  }
}
