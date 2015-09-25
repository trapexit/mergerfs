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
#include "fs_path.hpp"

#include "clone.hpp"

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
    return fs::path::basename(argv[0]);
  }

  static
  void
  get_fuse_operations(struct fuse_operations &ops)
  {
#if FLAG_NOPATH
    ops.flag_nopath      = false;
#endif
    ops.flag_nullpath_ok = false;

    ops.access      = mergerfs::fuse::access;
    ops.bmap        = NULL;
    ops.chmod       = mergerfs::fuse::chmod;
    ops.chown       = mergerfs::fuse::chown;
    ops.create      = mergerfs::fuse::create;
    ops.destroy     = mergerfs::fuse::destroy;
#if FALLOCATE
    ops.fallocate   = mergerfs::fuse::fallocate;
#endif
    ops.fgetattr    = mergerfs::fuse::fgetattr;
#if FLOCK
    ops.flock       = NULL;
#endif
    ops.flush       = mergerfs::fuse::flush;
    ops.fsync       = mergerfs::fuse::fsync;
    ops.fsyncdir    = NULL;
    ops.ftruncate   = mergerfs::fuse::ftruncate;
    ops.getattr     = mergerfs::fuse::getattr;
    ops.getdir      = NULL;       /* deprecated; use readdir */
    ops.getxattr    = mergerfs::fuse::getxattr;
    ops.init        = mergerfs::fuse::init;
    ops.ioctl       = mergerfs::fuse::ioctl;
    ops.link        = mergerfs::fuse::link;
    ops.listxattr   = mergerfs::fuse::listxattr;
    ops.lock        = NULL;
    ops.mkdir       = mergerfs::fuse::mkdir;
    ops.mknod       = mergerfs::fuse::mknod;
    ops.open        = mergerfs::fuse::open;
    ops.opendir     = mergerfs::fuse::opendir;
    ops.poll        = NULL;
    ops.read        = mergerfs::fuse::read;
#if READ_BUF
    ops.read_buf    = mergerfs::fuse::read_buf;
#endif
    ops.readdir     = mergerfs::fuse::readdir;
    ops.readlink    = mergerfs::fuse::readlink;
    ops.release     = mergerfs::fuse::release;
    ops.releasedir  = mergerfs::fuse::releasedir;
    ops.removexattr = mergerfs::fuse::removexattr;
    ops.rename      = mergerfs::fuse::rename;
    ops.rmdir       = mergerfs::fuse::rmdir;
    ops.setxattr    = mergerfs::fuse::setxattr;
    ops.statfs      = mergerfs::fuse::statfs;
    ops.symlink     = mergerfs::fuse::symlink;
    ops.truncate    = mergerfs::fuse::truncate;
    ops.unlink      = mergerfs::fuse::unlink;
    ops.utime       = NULL;       /* deprecated; use utimens() */
    ops.utimens     = mergerfs::fuse::utimens;
    ops.write       = mergerfs::fuse::write;
#if WRITE_BUF
    ops.write_buf   = mergerfs::fuse::write_buf;
#endif

    return;
  }

  static
  void
  setup_resources(void)
  {
    const int prio = -10;

    std::srand(time(NULL));
    mergerfs::resources::reset_umask();
    mergerfs::resources::maxout_rlimit_nofile();
    mergerfs::resources::maxout_rlimit_fsize();
    mergerfs::resources::setpriority(prio);
  }
}

namespace mergerfs
{
  int
  main(const int   argc,
       char      **argv)
  {
    fuse_args       args;
    fuse_operations ops = {0};
    Config          config;

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
  if(appname == "clone")
    return clonetool::main(argc,argv);

  return mergerfs::main(argc,argv);
}
