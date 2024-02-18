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

#include "config.hpp"
#include "ugid.hpp"
#include "fs_readahead.hpp"
#include "syslog.hpp"

#include "fmt/core.h"

#include "fuse.h"

#include <thread>


namespace l
{
  static
  void
  want(fuse_conn_info *conn_,
       const int       flag_)
  {
    conn_->want |= flag_;
  }

  static
  bool
  capable(fuse_conn_info *conn_,
          const int       flag_)
  {
    return !!(conn_->capable & flag_);
  }

  static
  void
  want_if_capable(fuse_conn_info *conn_,
                  const int       flag_)
  {
    if(capable(conn_,flag_))
      want(conn_,flag_);
  }

  static
  void
  want_if_capable(fuse_conn_info *conn_,
                  const int       flag_,
                  ConfigBOOL     *want_)
  {
    if(*want_ && l::capable(conn_,flag_))
      {
        l::want(conn_,flag_);
        return;
      }

    *want_ = false;
  }

  static
  void
  want_if_capable_max_pages(fuse_conn_info *conn_,
                            Config::Write  &cfg_)
  {
    if(l::capable(conn_,FUSE_CAP_MAX_PAGES))
      {
        l::want(conn_,FUSE_CAP_MAX_PAGES);
        conn_->max_pages = cfg_->fuse_msg_size;
      }
    else
      {
        cfg_->fuse_msg_size = FUSE_DEFAULT_MAX_PAGES_PER_REQ;
      }
  }

  static
  void
  readahead(const std::string path_,
            const int         readahead_)
  {
    int rv;

    rv = fs::readahead(path_,readahead_);
    if(rv == 0)
      syslog_info("%s - readahead set to %d",path_.c_str(),readahead_);
    else
      syslog_error("%s - unable to set readahead",path_.c_str());
  }

  static
  void
  set_readahead_on_mount_and_branches()
  {
    Config::Read cfg;
    Branches::CPtr branches;

    if((uint64_t)cfg->readahead == 0)
      return;

    l::readahead(cfg->mountpoint,cfg->readahead);

    branches = cfg->branches;
    for(auto const &branch : *branches)
      l::readahead(branch.path,cfg->readahead);
  }

  // Spawn a thread to do this because before init returns calls to
  // set the value will block leading to a deadlock. This is just
  // easier.
  static
  void
  spawn_thread_to_set_readahead()
  {
    std::thread readahead_thread(l::set_readahead_on_mount_and_branches);

    readahead_thread.detach();
  }
}

namespace FUSE
{
  void *
  init(fuse_conn_info *conn_)
  {
    Config::Write cfg;

    ugid::init();

    cfg->readdir.initialize();

    l::want_if_capable(conn_,FUSE_CAP_ASYNC_DIO);
    l::want_if_capable(conn_,FUSE_CAP_ASYNC_READ,&cfg->async_read);
    l::want_if_capable(conn_,FUSE_CAP_ATOMIC_O_TRUNC);
    l::want_if_capable(conn_,FUSE_CAP_BIG_WRITES);
    l::want_if_capable(conn_,FUSE_CAP_CACHE_SYMLINKS,&cfg->cache_symlinks);
    l::want_if_capable(conn_,FUSE_CAP_DONT_MASK);
    l::want_if_capable(conn_,FUSE_CAP_EXPORT_SUPPORT,&cfg->export_support);
    l::want_if_capable(conn_,FUSE_CAP_IOCTL_DIR);
    l::want_if_capable(conn_,FUSE_CAP_PARALLEL_DIROPS);
    l::want_if_capable(conn_,FUSE_CAP_POSIX_ACL,&cfg->posix_acl);
    l::want_if_capable(conn_,FUSE_CAP_READDIR_PLUS,&cfg->readdirplus);
    //    l::want_if_capable(conn_,FUSE_CAP_READDIR_PLUS_AUTO);
    l::want_if_capable(conn_,FUSE_CAP_WRITEBACK_CACHE,&cfg->writeback_cache);
    l::want_if_capable_max_pages(conn_,cfg);
    conn_->want &= ~FUSE_CAP_POSIX_LOCKS;
    conn_->want &= ~FUSE_CAP_FLOCK_LOCKS;

    l::spawn_thread_to_set_readahead();

    return NULL;
  }
}
