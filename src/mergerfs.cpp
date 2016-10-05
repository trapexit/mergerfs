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
#include <string.h>

#include <cstdlib>
#include <iostream>

#include "fs_path.hpp"
#include "mergerfs.hpp"
#include "option_parser.hpp"
#include "resources.hpp"

#include "access.hpp"
#include "chmod.hpp"
#include "chown.hpp"
#include "create.hpp"
#include "destroy.hpp"
#include "fallocate.hpp"
#include "fgetattr.hpp"
#include "flock.hpp"
#include "flush.hpp"
#include "fsync.hpp"
#include "fsyncdir.hpp"
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
  void
  get_fuse_operations(struct fuse_operations &ops,
                      const bool              direct_io,
                      const bool              nullrw)
  {
    ops.flag_nullpath_ok   = true;
    ops.flag_nopath        = true;
    ops.flag_utime_omit_ok = true;

    ops.access      = mergerfs::fuse::access;
    ops.bmap        = NULL;
    ops.chmod       = mergerfs::fuse::chmod;
    ops.chown       = mergerfs::fuse::chown;
    ops.create      = mergerfs::fuse::create;
    ops.destroy     = mergerfs::fuse::destroy;
    ops.fallocate   = mergerfs::fuse::fallocate;
    ops.fgetattr    = mergerfs::fuse::fgetattr;
    ops.flock       = mergerfs::fuse::flock;
    ops.flush       = mergerfs::fuse::flush;
    ops.fsync       = mergerfs::fuse::fsync;
    ops.fsyncdir    = mergerfs::fuse::fsyncdir;
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
    ops.read        = (nullrw ?
                       mergerfs::fuse::read_null :
                       (direct_io ?
                        mergerfs::fuse::read_direct_io :
                        mergerfs::fuse::read));
    ops.read_buf    = (nullrw ?
                       NULL :
                       mergerfs::fuse::read_buf);
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
    ops.write       = (nullrw ?
                       mergerfs::fuse::write_null :
                       (direct_io ?
                        mergerfs::fuse::write_direct_io :
                        mergerfs::fuse::write));
    ops.write_buf   = (nullrw ?
                       mergerfs::fuse::write_buf_null :
                       mergerfs::fuse::write_buf);

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
    local::get_fuse_operations(ops,
                               config.direct_io,
                               config.nullrw);

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
  return mergerfs::main(argc,argv);
}
