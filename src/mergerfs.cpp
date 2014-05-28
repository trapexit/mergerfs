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
#include <string.h>

#include <cstdlib>
#include <iostream>

#include "mergerfs.hpp"
#include "option_parser.hpp"
#include "resources.hpp"
#include "fs.hpp"
#include "test.hpp"

#include "access.hpp"
#include "chmod.hpp"
#include "chown.hpp"
#include "create.hpp"
#include "fallocate.hpp"
#include "fsync.hpp"
#include "ftruncate.hpp"
#include "getattr.hpp"
#include "getxattr.hpp"
#include "ioctl.hpp"
#include "link.hpp"
#include "listxattr.hpp"
#include "mkdir.hpp"
#include "mknod.hpp"
#include "open.hpp"
#include "read.hpp"
#include "readdir.hpp"
#include "readlink.hpp"
#include "release.hpp"
#include "removexattr.hpp"
#include "rename.hpp"
#include "rmdir.hpp"
#include "setxattr.hpp"
#include "statfs.hpp"
#include "symlink.hpp"
#include "truncate.hpp"
#include "unlink.hpp"
#include "utimens.hpp"
#include "write.hpp"

static
struct fuse_operations
get_fuse_operations()
{
  struct fuse_operations ops = {};

  ops.flag_nopath      = false;
  ops.flag_nullpath_ok = false;

  ops.access      = mergerfs::access::access;
  ops.bmap        = NULL;
  ops.chmod       = mergerfs::chmod::chmod;
  ops.chown       = mergerfs::chown::chown;
  ops.create      = mergerfs::create::create;
  ops.destroy     = NULL;
  ops.fallocate   = mergerfs::fallocate::fallocate;
  ops.fgetattr    = NULL;
  ops.flock       = NULL;
  ops.flush       = NULL;       /* called on close() of fd */
  ops.fsync       = mergerfs::fsync::fsync;
  ops.fsyncdir    = NULL;
  ops.ftruncate   = mergerfs::ftruncate::ftruncate;
  ops.getattr     = mergerfs::getattr::getattr;
  ops.getdir      = NULL;       /* deprecated; use readdir */
  ops.getxattr    = mergerfs::getxattr::getxattr;
  ops.init        = NULL;
  ops.ioctl       = mergerfs::ioctl::ioctl;
  ops.link        = mergerfs::link::link;
  ops.listxattr   = mergerfs::listxattr::listxattr;
  ops.lock        = NULL;
  ops.mkdir       = mergerfs::mkdir::mkdir;
  ops.mknod       = mergerfs::mknod::mknod;
  ops.open        = mergerfs::open::open;
  ops.opendir     = NULL;
  ops.poll        = NULL;
  ops.read        = mergerfs::read::read;
  ops.read_buf    = NULL;
  ops.readdir     = mergerfs::readdir::readdir;
  ops.readlink    = mergerfs::readlink::readlink;
  ops.release     = mergerfs::release::release;
  ops.releasedir  = NULL;
  ops.removexattr = mergerfs::removexattr::removexattr;
  ops.rename      = mergerfs::rename::rename;
  ops.rmdir       = mergerfs::rmdir::rmdir;
  ops.setxattr    = mergerfs::setxattr::setxattr;
  ops.statfs      = mergerfs::statfs::statfs;
  ops.symlink     = mergerfs::symlink::symlink;
  ops.truncate    = mergerfs::truncate::truncate;
  ops.unlink      = mergerfs::unlink::unlink;
  ops.utime       = NULL;       /* deprecated; use utimens() */
  ops.utimens     = mergerfs::utimens::utimens;
  ops.write       = mergerfs::write::write;
  ops.write_buf   = NULL;

  return ops;
}

namespace mergerfs
{
  int
  main(const struct fuse_args &args,
       config::Config         &config)
  {
    struct fuse_operations ops;

    ops = get_fuse_operations();

    std::srand(time(NULL));
    resources::reset_umask();
    resources::maxout_rlimit_nofile();
    resources::maxout_rlimit_fsize();

    return fuse_main(args.argc,
                     args.argv,
                     &ops,
                     &config);
  }
}

int
main(int   argc,
     char *argv[])
{
  int rv;
  mergerfs::config::Config config;
  struct fuse_args args = FUSE_ARGS_INIT(argc,argv);

  mergerfs::options::parse(args,config);

  if(config.testmode == false)
    rv = mergerfs::main(args,config);
  else
    rv = mergerfs::test(args,config);

  return rv;
}
