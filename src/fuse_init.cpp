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

#include "fuse.h"


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
}

namespace FUSE
{
  void *
  init(fuse_conn_info *conn_)
  {
    Config::Write cfg;

    ugid::init();

    l::want_if_capable(conn_,FUSE_CAP_ASYNC_DIO);
    l::want_if_capable(conn_,FUSE_CAP_ASYNC_READ,&cfg->async_read);
    l::want_if_capable(conn_,FUSE_CAP_ATOMIC_O_TRUNC);
    l::want_if_capable(conn_,FUSE_CAP_BIG_WRITES);
    l::want_if_capable(conn_,FUSE_CAP_CACHE_SYMLINKS,&cfg->cache_symlinks);
    l::want_if_capable(conn_,FUSE_CAP_DONT_MASK);
    l::want_if_capable(conn_,FUSE_CAP_IOCTL_DIR);
    l::want_if_capable(conn_,FUSE_CAP_PARALLEL_DIROPS);
    l::want_if_capable(conn_,FUSE_CAP_READDIR_PLUS,&cfg->readdirplus);
    //l::want_if_capable(conn_,FUSE_CAP_READDIR_PLUS_AUTO);
    l::want_if_capable(conn_,FUSE_CAP_POSIX_ACL,&cfg->posix_acl);
    l::want_if_capable(conn_,FUSE_CAP_WRITEBACK_CACHE,&cfg->writeback_cache);
    l::want_if_capable_max_pages(conn_,cfg);

    return NULL;
  }
}
