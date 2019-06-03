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

#include "fs_path.hpp"
#include "mergerfs.hpp"
#include "option_parser.hpp"
#include "resources.hpp"

#include "fuse_access.hpp"
#include "fuse_chmod.hpp"
#include "fuse_chown.hpp"
#include "fuse_copy_file_range.hpp"
#include "fuse_create.hpp"
#include "fuse_destroy.hpp"
#include "fuse_fallocate.hpp"
#include "fuse_fchmod.hpp"
#include "fuse_fchown.hpp"
#include "fuse_fgetattr.hpp"
#include "fuse_flock.hpp"
#include "fuse_flush.hpp"
#include "fuse_free_hide.hpp"
#include "fuse_fsync.hpp"
#include "fuse_fsyncdir.hpp"
#include "fuse_ftruncate.hpp"
#include "fuse_futimens.hpp"
#include "fuse_getattr.hpp"
#include "fuse_getxattr.hpp"
#include "fuse_init.hpp"
#include "fuse_ioctl.hpp"
#include "fuse_link.hpp"
#include "fuse_listxattr.hpp"
#include "fuse_mkdir.hpp"
#include "fuse_mknod.hpp"
#include "fuse_open.hpp"
#include "fuse_opendir.hpp"
#include "fuse_prepare_hide.hpp"
#include "fuse_read.hpp"
#include "fuse_read_buf.hpp"
#include "fuse_readdir.hpp"
#include "fuse_readlink.hpp"
#include "fuse_release.hpp"
#include "fuse_releasedir.hpp"
#include "fuse_removexattr.hpp"
#include "fuse_rename.hpp"
#include "fuse_rmdir.hpp"
#include "fuse_setxattr.hpp"
#include "fuse_statfs.hpp"
#include "fuse_symlink.hpp"
#include "fuse_truncate.hpp"
#include "fuse_unlink.hpp"
#include "fuse_utimens.hpp"
#include "fuse_write.hpp"
#include "fuse_write_buf.hpp"

#include <fuse.h>

#include <cstdlib>
#include <iostream>

#include <string.h>

namespace l
{
  static
  void
  get_fuse_operations(struct fuse_operations &ops,
                      const bool              nullrw)
  {
    ops.flag_nullpath_ok   = true;
    ops.flag_nopath        = true;
    ops.flag_utime_omit_ok = true;

    ops.access          = FUSE::access;
    ops.bmap            = NULL;
    ops.chmod           = FUSE::chmod;
    ops.chown           = FUSE::chown;
    ops.copy_file_range = FUSE::copy_file_range;
    ops.create          = FUSE::create;
    ops.destroy         = FUSE::destroy;
    ops.fallocate       = FUSE::fallocate;
    ops.fchmod          = FUSE::fchmod;
    ops.fchown          = FUSE::fchown;
    ops.fgetattr        = FUSE::fgetattr;
    ops.flock           = NULL; // FUSE::flock;
    ops.flush           = FUSE::flush;
    ops.free_hide       = FUSE::free_hide;
    ops.fsync           = FUSE::fsync;
    ops.fsyncdir        = FUSE::fsyncdir;
    ops.ftruncate       = FUSE::ftruncate;
    ops.futimens        = FUSE::futimens;
    ops.getattr         = FUSE::getattr;
    ops.getdir          = NULL; /* deprecated; use readdir */
    ops.getxattr        = FUSE::getxattr;
    ops.init            = FUSE::init;
    ops.ioctl           = FUSE::ioctl;
    ops.link            = FUSE::link;
    ops.listxattr       = FUSE::listxattr;
    ops.lock            = NULL;
    ops.mkdir           = FUSE::mkdir;
    ops.mknod           = FUSE::mknod;
    ops.open            = FUSE::open;
    ops.opendir         = FUSE::opendir;
    ops.poll            = NULL;
    ops.prepare_hide    = FUSE::prepare_hide;
    ops.read            = (nullrw ? FUSE::read_null : FUSE::read);
    ops.read_buf        = (nullrw ? NULL : FUSE::read_buf);
    ops.readdir         = FUSE::readdir;
    ops.readlink        = FUSE::readlink;
    ops.release         = FUSE::release;
    ops.releasedir      = FUSE::releasedir;
    ops.removexattr     = FUSE::removexattr;
    ops.rename          = FUSE::rename;
    ops.rmdir           = FUSE::rmdir;
    ops.setxattr        = FUSE::setxattr;
    ops.statfs          = FUSE::statfs;
    ops.symlink         = FUSE::symlink;
    ops.truncate        = FUSE::truncate;
    ops.unlink          = FUSE::unlink;
    ops.utime           = NULL; /* deprecated; use utimens() */
    ops.utimens         = FUSE::utimens;
    ops.write           = (nullrw ? FUSE::write_null : FUSE::write);
    ops.write_buf       = (nullrw ? FUSE::write_buf_null : FUSE::write_buf);

    return;
  }

  static
  void
  setup_resources(void)
  {
    const int prio = -10;

    std::srand(time(NULL));
    resources::reset_umask();
    resources::maxout_rlimit_nofile();
    resources::maxout_rlimit_fsize();
    resources::setpriority(prio);
  }

  int
  main(const int   argc_,
       char      **argv_)
  {
    fuse_args       args;
    fuse_operations ops = {0};
    Config          config;

    args.argc      = argc_;
    args.argv      = argv_;
    args.allocated = 0;

    options::parse(&args,&config);

    l::setup_resources();
    l::get_fuse_operations(ops,config.nullrw);

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
  return l::main(argc,argv);
}
