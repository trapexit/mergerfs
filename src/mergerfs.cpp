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

#include "fs_readahead.hpp"
#include "fs_wait_for_mount.hpp"
#include "syslog.hpp"

#include "fs_path.hpp"
#include "fs_umount2.hpp"
#include "mergerfs.hpp"
#include "option_parser.hpp"
#include "procfs_get_name.hpp"
#include "resources.hpp"
#include "strvec.hpp"
#include "gidcache.hpp"

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
#include "fuse_read.hpp"
#include "fuse_readdir.hpp"
#include "fuse_readdir_plus.hpp"
#include "fuse_readlink.hpp"
#include "fuse_release.hpp"
#include "fuse_releasedir.hpp"
#include "fuse_removemapping.hpp"
#include "fuse_removexattr.hpp"
#include "fuse_rename.hpp"
#include "fuse_rmdir.hpp"
#include "fuse_setupmapping.hpp"
#include "fuse_setxattr.hpp"
#include "fuse_statfs.hpp"
#include "fuse_symlink.hpp"
#include "fuse_syncfs.hpp"
#include "fuse_tmpfile.hpp"
#include "fuse_truncate.hpp"
#include "fuse_unlink.hpp"
#include "fuse_utimens.hpp"
#include "fuse_write.hpp"

#include "fuse.h"

#include <csignal>
#include <cstdlib>
#include <iostream>

#include <string.h>


namespace l
{
  static
  void
  get_fuse_operations(struct fuse_operations &ops_,
                      const bool              nullrw_)
  {
    ops_.access          = FUSE::access;
    ops_.bmap            = FUSE::bmap;
    ops_.chmod           = FUSE::chmod;
    ops_.chown           = FUSE::chown;
    ops_.copy_file_range = FUSE::copy_file_range;
    ops_.create          = FUSE::create;
    ops_.destroy         = FUSE::destroy;
    ops_.fallocate       = FUSE::fallocate;
    ops_.fchmod          = FUSE::fchmod;
    ops_.fchown          = FUSE::fchown;
    ops_.fgetattr        = FUSE::fgetattr;
    ops_.flock           = FUSE::flock;
    ops_.flush           = FUSE::flush;
    ops_.free_hide       = FUSE::free_hide;
    ops_.fsync           = FUSE::fsync;
    ops_.fsyncdir        = FUSE::fsyncdir;
    ops_.ftruncate       = FUSE::ftruncate;
    ops_.futimens        = FUSE::futimens;
    ops_.getattr         = FUSE::getattr;
    ops_.getxattr        = FUSE::getxattr;
    ops_.init            = FUSE::init;
    ops_.ioctl           = FUSE::ioctl;
    ops_.link            = FUSE::link;
    ops_.listxattr       = FUSE::listxattr;
    ops_.lock            = FUSE::lock;
    ops_.mkdir           = FUSE::mkdir;
    ops_.mknod           = FUSE::mknod;
    ops_.open            = FUSE::open;
    ops_.opendir         = FUSE::opendir;
    ops_.poll            = FUSE::poll;;
    ops_.prepare_hide    = FUSE::prepare_hide;
    ops_.read            = (nullrw_ ? FUSE::read_null : FUSE::read);
    ops_.readdir         = FUSE::readdir;
    ops_.readdir_plus    = FUSE::readdir_plus;
    ops_.readlink        = FUSE::readlink;
    ops_.release         = FUSE::release;
    ops_.releasedir      = FUSE::releasedir;
    ops_.removemapping   = FUSE::removemapping;
    ops_.removexattr     = FUSE::removexattr;
    ops_.rename          = FUSE::rename;
    ops_.rmdir           = FUSE::rmdir;
    ops_.setupmapping    = FUSE::setupmapping;
    ops_.setxattr        = FUSE::setxattr;
    ops_.statfs          = FUSE::statfs;
    ops_.symlink         = FUSE::symlink;
    ops_.syncfs          = FUSE::syncfs;
    ops_.tmpfile         = FUSE::tmpfile;
    ops_.truncate        = FUSE::truncate;
    ops_.unlink          = FUSE::unlink;
    ops_.utimens         = FUSE::utimens;
    ops_.write           = (nullrw_ ? FUSE::write_null : FUSE::write);

    return;
  }

  static
  void
  setup_resources(const int scheduling_priority_)
  {
    std::srand(time(NULL));
    resources::reset_umask();
    resources::maxout_rlimit_nofile();
    resources::maxout_rlimit_fsize();
    resources::setpriority(scheduling_priority_);
  }

  static
  void
  wait_for_mount(const Config::Read &cfg_)
  {
    fs::PathVector paths;
    std::chrono::milliseconds timeout;

    paths = cfg_->branches->to_paths();

    syslog_info("Waiting %u seconds for branches to mount",
                (uint64_t)cfg_->branches_mount_timeout);

    timeout = std::chrono::milliseconds(cfg_->branches_mount_timeout * 1000);
    fs::wait_for_mount((std::string)cfg_->mountpoint,
                       paths,
                       timeout);
  }

  static
  void
  lazy_umount(const std::string target_)
  {
    int rv;

    rv = fs::umount_lazy(target_);
    switch(rv)
      {
      case 0:
        syslog_notice("%s has been successfully lazily unmounted",
                      target_.c_str());
        break;
      case -EINVAL:
        syslog_notice("%s was not a mount point needing to be unmounted",
                      target_.c_str());
        break;
      default:
        syslog_error("Error unmounting %s: %d - %s",
                     target_.c_str(),
                     -rv,
                     strerror(-rv));
        break;
      }
  }

  static
  void
  usr1_signal_handler(int signal_)
  {
    syslog_info("Received SIGUSR1 - invalidating all nodes");
    fuse_invalidate_all_nodes();
  }

  static
  void
  usr2_signal_handler(int signal_)
  {
    syslog_info("Received SIGUSR2 - triggering thorough gc");
    fuse_gc();
    GIDCache::invalidate_all_caches();
  }

  static
  void
  setup_signal_handlers()
  {
    std::signal(SIGUSR1,l::usr1_signal_handler);
    std::signal(SIGUSR2,l::usr2_signal_handler);
  }

  static
  void
  warn_if_not_root()
  {
    uid_t uid;

    uid = geteuid();
    if(uid == 0)
      return;

    char const *s = "mergerfs is not running as root and may not work correctly\n";
    fprintf(stderr,"warning: %s",s);
    syslog_warning(s);
  }

  int
  main(const int   argc_,
       char      **argv_)
  {
    int rv;
    Config::Read    cfg;
    Config::ErrVec  errs;
    fuse_args       args;
    fuse_operations ops;

    syslog_open();

    l::warn_if_not_root();

    memset(&ops,0,sizeof(fuse_operations));

    args.argc      = argc_;
    args.argv      = argv_;
    args.allocated = 0;

    options::parse(&args,&errs);
    if(errs.size())
      {
        std::cerr << errs << std::endl;
        return 1;
      }

    if(cfg->branches_mount_timeout > 0)
      l::wait_for_mount(cfg);

    l::setup_resources(cfg->scheduling_priority);
    l::setup_signal_handlers();
    l::get_fuse_operations(ops,cfg->nullrw);

    if(cfg->lazy_umount_mountpoint)
      l::lazy_umount(cfg->mountpoint);

    procfs::init();

    rv = fuse_main(args.argc,
                   args.argv,
                   &ops);

    syslog_info("exiting main loop with return code %d",rv);

    return rv;
  }
}

int
main(int    argc_,
     char **argv_)
{
  return l::main(argc_,argv_);
}
