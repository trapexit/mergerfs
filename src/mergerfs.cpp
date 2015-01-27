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

#include "clonepath.hpp"

#include "access.hpp"
#include "chmod.hpp"
#include "chown.hpp"
#include "create.hpp"
#include "destroy.hpp"
#include "fallocate.hpp"
#include "fgetattr.hpp"
#include "flush.hpp"
#include "fsync.hpp"
#include "ftruncate.hpp"
#include "getattr.hpp"
#include "getxattr.hpp"
#include "init.hpp"
#include "ioctl.hpp"
#include "link.hpp"
#include "listxattr.hpp"
#include "mkdir.hpp"
#include "mknod.hpp"
#include "open.hpp"
#include "opendir.hpp"
#include "read.hpp"
#include "read_buf.hpp"
#include "readdir.hpp"
#include "readlink.hpp"
#include "release.hpp"
#include "releasedir.hpp"
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
#include "write_buf.hpp"

namespace local
{
  static
  std::string
  getappname(const int     argc,
             char * const *argv)
  {
    return fs::basename(argv[0]);
  }
  
  static
  void
  get_fuse_operations(struct fuse_operations &ops)
  {
#if FLAG_NOPATH
    ops.flag_nopath      = false;
#endif
    ops.flag_nullpath_ok = false;

    ops.access      = mergerfs::access::access;
    ops.bmap        = NULL;
    ops.chmod       = mergerfs::chmod::chmod;
    ops.chown       = mergerfs::chown::chown;
    ops.create      = mergerfs::create::create;
    ops.destroy     = mergerfs::destroy::destroy;
#if FALLOCATE
    ops.fallocate   = mergerfs::fallocate::fallocate;
#endif
    ops.fgetattr    = mergerfs::fgetattr::fgetattr;
#if FLOCK
    ops.flock       = NULL;
#endif
    ops.flush       = mergerfs::flush::flush;
    ops.fsync       = mergerfs::fsync::fsync;
    ops.fsyncdir    = NULL;
    ops.ftruncate   = mergerfs::ftruncate::ftruncate;
    ops.getattr     = mergerfs::getattr::getattr;
    ops.getdir      = NULL;       /* deprecated; use readdir */
    ops.getxattr    = mergerfs::getxattr::getxattr;
    ops.init        = mergerfs::init::init;
    ops.ioctl       = mergerfs::ioctl::ioctl;
    ops.link        = mergerfs::link::link;
    ops.listxattr   = mergerfs::listxattr::listxattr;
    ops.lock        = NULL;
    ops.mkdir       = mergerfs::mkdir::mkdir;
    ops.mknod       = mergerfs::mknod::mknod;
    ops.open        = mergerfs::open::open;
    ops.opendir     = mergerfs::opendir::opendir;
    ops.poll        = NULL;
    ops.read        = mergerfs::read::read;
#if READ_BUF
    ops.read_buf    = mergerfs::read_buf::read_buf;
#endif
    ops.readdir     = mergerfs::readdir::readdir;
    ops.readlink    = mergerfs::readlink::readlink;
    ops.release     = mergerfs::release::release;
    ops.releasedir  = mergerfs::releasedir::releasedir;
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
#if WRITE_BUF
    ops.write_buf   = mergerfs::write_buf::write_buf;
#endif

    return;
  }

  static
  void
  setup_resources(void)
  {
    std::srand(time(NULL));
    mergerfs::resources::reset_umask();
    mergerfs::resources::maxout_rlimit_nofile();
    mergerfs::resources::maxout_rlimit_fsize();
  }
}

namespace mergerfs
{
  int
  main(const int   argc,
       char      **argv)
  {
    struct fuse_args         args;
    struct fuse_operations   ops = {0};
    mergerfs::config::Config config;

    args.argc      = argc;
    args.argv      = argv;
    args.allocated = 0;

    mergerfs::options::parse(args,config);

    local::setup_resources();
    local::get_fuse_operations(ops);

    return fuse_main(args.argc,
                     args.argv,
                     &ops,
                     &config);
  }
}

int
main(int    argc,
     char **argv)
{
  std::string appname;

  appname = local::getappname(argc,argv);
  if(appname == "clonepath")
    return clonepath::main(argc,argv);

  return mergerfs::main(argc,argv);
}
