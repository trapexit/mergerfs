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

#include "ugid.hpp"
#include "state.hpp"

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
                  bool           *want_)
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
                            State &s_)
  {
    if(l::capable(conn_,FUSE_CAP_MAX_PAGES))
      {
        l::want(conn_,FUSE_CAP_MAX_PAGES);
        conn_->max_pages = s_->fuse_msg_size;
      }
    else
      {
        s_->fuse_msg_size = FUSE_DEFAULT_MAX_PAGES_PER_REQ;
      }
  }
}

namespace FUSE::INIT
{
  void *
  init(fuse_conn_info *conn_)
  {
    State s;
    bool async_read;
    bool posix_acl;
    bool wb_cache;
    bool readdirplus;
    bool cache_symlinks;

    async_read     = toml::find_or(s->config_toml,"fuse","async-read",true);
    posix_acl      = toml::find_or(s->config_toml,"fuse","posix-acl",false);
    wb_cache       = toml::find_or(s->config_toml,"cache","writeback",false);
    readdirplus    = toml::find_or(s->config_toml,"func","readdir","readdirplus",false);
    cache_symlinks = toml::find_or(s->config_toml,"cache","symlinks",false);
    
    ugid::init();

    l::want_if_capable(conn_,FUSE_CAP_ASYNC_DIO);
    l::want_if_capable(conn_,FUSE_CAP_ASYNC_READ,&async_read);
    l::want_if_capable(conn_,FUSE_CAP_ATOMIC_O_TRUNC);
    l::want_if_capable(conn_,FUSE_CAP_BIG_WRITES);
    l::want_if_capable(conn_,FUSE_CAP_CACHE_SYMLINKS,&cache_symlinks);
    l::want_if_capable(conn_,FUSE_CAP_DONT_MASK);
    l::want_if_capable(conn_,FUSE_CAP_IOCTL_DIR);
    l::want_if_capable(conn_,FUSE_CAP_PARALLEL_DIROPS);
    l::want_if_capable(conn_,FUSE_CAP_READDIR_PLUS,&readdirplus);
    //l::want_if_capable(conn_,FUSE_CAP_READDIR_PLUS_AUTO);
    l::want_if_capable(conn_,FUSE_CAP_POSIX_ACL,&posix_acl);
    l::want_if_capable(conn_,FUSE_CAP_WRITEBACK_CACHE,&wb_cache);
    l::want_if_capable_max_pages(conn_,s);
    conn_->want &= ~FUSE_CAP_POSIX_LOCKS;
    conn_->want &= ~FUSE_CAP_FLOCK_LOCKS;

    return NULL;
  }
}
