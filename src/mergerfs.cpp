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
#include "resources.hpp"
#include "strvec.hpp"

#include "state.hpp"

#include "fuse_access.hpp"
#include "fuse_bmap.hpp"
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
#include "fuse_lock.hpp"
#include "fuse_mkdir.hpp"
#include "fuse_mknod.hpp"
#include "fuse_open.hpp"
#include "fuse_opendir.hpp"
#include "fuse_poll.hpp"
#include "fuse_prepare_hide.hpp"
#include "fuse_read_buf.hpp"
#include "fuse_readdir.hpp"
#include "fuse_readdirplus.hpp"
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
#include "fuse_write_buf.hpp"

#include "fuse.h"

#include "toml.hpp"
#include "toml_verify.hpp"

#include <cstdlib>
#include <iostream>

#include <string.h>


namespace l
{
  static
  void
  get_fuse_operations(struct fuse_operations &ops_)
  {
    ops_.access          = FUSE::ACCESS::access;
    ops_.bmap            = FUSE::BMAP::bmap;
    ops_.chmod           = FUSE::CHMOD::chmod;
    ops_.chown           = FUSE::CHOWN::chown;
    ops_.copy_file_range = FUSE::COPY_FILE_RANGE::copy_file_range;
    ops_.create          = FUSE::CREATE::create;
    ops_.destroy         = FUSE::DESTROY::destroy;
    ops_.fallocate       = FUSE::FALLOCATE::fallocate;
    ops_.fchmod          = FUSE::FCHMOD::fchmod;
    ops_.fchown          = FUSE::FCHOWN::fchown;
    ops_.fgetattr        = FUSE::FGETATTR::fgetattr;
    ops_.flock           = FUSE::FLOCK::flock;
    ops_.flush           = FUSE::FLUSH::flush;
    ops_.free_hide       = FUSE::FREE_HIDE::free_hide;
    ops_.fsync           = FUSE::FSYNC::fsync;
    ops_.fsyncdir        = FUSE::FSYNCDIR::fsyncdir;
    ops_.ftruncate       = FUSE::FTRUNCATE::ftruncate;
    ops_.futimens        = FUSE::FUTIMENS::futimens;
    ops_.getattr         = FUSE::GETATTR::getattr;
    ops_.getxattr        = FUSE::GETXATTR::getxattr;
    ops_.init            = FUSE::INIT::init;
    ops_.ioctl           = FUSE::IOCTL::ioctl;
    ops_.link            = FUSE::LINK::link;
    ops_.listxattr       = FUSE::LISTXATTR::listxattr;
    ops_.lock            = FUSE::LOCK::lock;
    ops_.mkdir           = FUSE::MKDIR::mkdir;
    ops_.mknod           = FUSE::MKNOD::mknod;
    ops_.open            = FUSE::OPEN::open;
    ops_.opendir         = FUSE::OPENDIR::opendir;
    ops_.poll            = FUSE::POLL::poll;;
    ops_.prepare_hide    = FUSE::PREPARE_HIDE::prepare_hide;
    //    ops_.read_buf        = FUSE::READ::read;
    ops_.readdir         = FUSE::READDIR::readdir;
    ops_.readdir_plus    = FUSE::READDIRPLUS::readdirplus;
    ops_.readlink        = FUSE::READLINK::readlink;
    ops_.release         = FUSE::RELEASE::release;
    ops_.releasedir      = FUSE::RELEASEDIR::releasedir;
    ops_.removexattr     = FUSE::REMOVEXATTR::removexattr;
    ops_.rename          = FUSE::RENAME::rename;
    ops_.rmdir           = FUSE::RMDIR::rmdir;
    ops_.setxattr        = FUSE::SETXATTR::setxattr;
    ops_.statfs          = FUSE::STATFS::statfs;
    ops_.symlink         = FUSE::SYMLINK::symlink;
    ops_.truncate        = FUSE::TRUNCATE::truncate;
    ops_.unlink          = FUSE::UNLINK::unlink;
    ops_.utimens         = FUSE::UTIMENS::utimens;
    //    ops_.write_buf       = FUSE::WRITE::write;

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
    fuse_operations ops;

    memset(&ops,0,sizeof(fuse_operations));

    args.argc      = 0;
    args.argv      = NULL;
    args.allocated = 1;

    for(int i = 0; i < argc_; i++)
      fuse_opt_add_arg(&args,argv_[i]);
    fuse_opt_add_arg(&args,"/tmp/test");

    l::setup_resources();
    l::get_fuse_operations(ops);

    return fuse_main(args.argc,
                     args.argv,
                     &ops);
  }
}

int
main(int    argc_,
     char **argv_)
{
  State s;
  std::vector<std::string> errs;

  const auto doc = toml::parse("config.toml");

  s = doc;

  try
    {
      //toml::verify(doc,errs);
    }
  catch(const toml::exception &e)
    {
      std::cout << e.what()
                << std::endl;
    }

  for(const auto &err : errs)
    {
      std::cout << err << std::endl;
    }

  return l::main(argc_,argv_);
}
