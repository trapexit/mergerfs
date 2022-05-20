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

#include <iostream>


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
                  const int       flag_,
                  const char     *str_)
  {
    if(capable(conn_,flag_))
      want(conn_,flag_);
    else
      fprintf(stderr,"mergerfs: warning - want to enable %s but not available\n",str_);
  }

  static
  void
  want_if_capable_max_pages(fuse_conn_info *conn_,
                            uint64_t        max_pages_)
  {
    if(l::capable(conn_,FUSE_CAP_MAX_PAGES))
      {
        l::want(conn_,FUSE_CAP_MAX_PAGES);
        conn_->max_pages = max_pages_;
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
    uint64_t max_pages;

    async_read     = toml::find_or(s->toml,"fuse","async-read",true);
    posix_acl      = toml::find_or(s->toml,"fuse","posix-acl",false);
    wb_cache       = toml::find_or(s->toml,"cache","writeback",false);
    readdirplus    = toml::find_or(s->toml,"func","readdir","readdirplus",false);
    cache_symlinks = toml::find_or(s->toml,"cache","symlinks",false);
    max_pages      = toml::find_or(s->toml,"fuse","message-size",256);

    ugid::init();

    l::want_if_capable(conn_,FUSE_CAP_ASYNC_DIO,"ASYNC_DIO");
    l::want_if_capable(conn_,FUSE_CAP_ATOMIC_O_TRUNC,"ATOMIC_O_TRUNC");
    l::want_if_capable(conn_,FUSE_CAP_BIG_WRITES,"BIG_WRITES");
    l::want_if_capable(conn_,FUSE_CAP_DONT_MASK,"DONT_MASK");
    l::want_if_capable(conn_,FUSE_CAP_IOCTL_DIR,"IOCTL_DIR");
    l::want_if_capable(conn_,FUSE_CAP_PARALLEL_DIROPS,"PARALLEL_DIROPS");
    l::want_if_capable_max_pages(conn_,max_pages);
    //l::want_if_capable(conn_,FUSE_CAP_READDIR_PLUS_AUTO,"READIR_PLUS_AUTO");
    if(async_read)
      l::want_if_capable(conn_,FUSE_CAP_ASYNC_READ,"ASYNC_READ");
    if(cache_symlinks)
      l::want_if_capable(conn_,FUSE_CAP_CACHE_SYMLINKS,"CACHE_SYMLINKS");
    if(readdirplus)
      l::want_if_capable(conn_,FUSE_CAP_READDIR_PLUS,"READDIR_PLUS");
    if(posix_acl)
      l::want_if_capable(conn_,FUSE_CAP_POSIX_ACL,"POSIX_ACL");
    if(wb_cache)
      l::want_if_capable(conn_,FUSE_CAP_WRITEBACK_CACHE,"WRITEBACK_CACHE");
    conn_->want &= ~FUSE_CAP_POSIX_LOCKS;
    conn_->want &= ~FUSE_CAP_FLOCK_LOCKS;

    return NULL;
  }
}
