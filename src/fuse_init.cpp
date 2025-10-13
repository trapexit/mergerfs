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

#include "fuse_init.hpp"

#include "config.hpp"
#include "fs_readahead.hpp"
#include "procfs.hpp"
#include "state.hpp"
#include "syslog.hpp"
#include "ugid.hpp"

#include "fs_path.hpp"
#include "fs_exists.hpp"

#include "fuse.h"

#include <algorithm>
#include <fstream>
#include <thread>


static
void
_want(fuse_conn_info *conn_,
      const int       flag_)
{
  conn_->want |= flag_;
}

static
bool
_capable(fuse_conn_info *conn_,
         const int       flag_)
{
  return !!(conn_->capable & flag_);
}

static
void
_want_if_capable(fuse_conn_info *conn_,
                 const int       flag_)
{
  if(::_capable(conn_,flag_))
    ::_want(conn_,flag_);
}

static
void
_want_if_capable(fuse_conn_info *conn_,
                 const int       flag_,
                 ConfigBOOL     *want_)
{
  if(*want_ && ::_capable(conn_,flag_))
    {
      ::_want(conn_,flag_);
      return;
    }

  *want_ = false;
}

#define MAX_FUSE_MSG_SIZE 65535
static const char MAX_PAGES_LIMIT_FILEPATH[] = "/proc/sys/fs/fuse/max_pages_limit";

static
void
_want_if_capable_max_pages(fuse_conn_info *conn_,
                           Config         &cfg_)
{
  std::fstream f;
  uint64_t max_pages_limit;

  if(fs::exists(MAX_PAGES_LIMIT_FILEPATH))
    {
      if(cfg_.fuse_msg_size > MAX_FUSE_MSG_SIZE)
        SysLog::info("fuse_msg_size > {}: setting it to {}",
                     MAX_FUSE_MSG_SIZE,
                     MAX_FUSE_MSG_SIZE);
      cfg_.fuse_msg_size = std::min((uint64_t)cfg_.fuse_msg_size,
                                    (uint64_t)MAX_FUSE_MSG_SIZE);

      f.open(MAX_PAGES_LIMIT_FILEPATH,f.in|f.out);
      if(f.is_open())
        {
          f >> max_pages_limit;
          SysLog::info("{} currently set to {}",
                       MAX_PAGES_LIMIT_FILEPATH,
                       max_pages_limit);
          if(cfg_.fuse_msg_size > max_pages_limit)
            {
              f.seekp(0);
              f << (uint64_t)cfg_.fuse_msg_size;
              f.flush();
              SysLog::info("{} changed to {}",
                           MAX_PAGES_LIMIT_FILEPATH,
                           (uint64_t)cfg_.fuse_msg_size);
            }
          f.close();
        }
      else
        {
          if(cfg_.fuse_msg_size != FUSE_DEFAULT_MAX_MAX_PAGES)
            SysLog::info("unable to open {}",MAX_PAGES_LIMIT_FILEPATH);
        }
    }
  else
    {
      if(cfg_.fuse_msg_size > FUSE_DEFAULT_MAX_MAX_PAGES)
        SysLog::info("fuse_msg_size request {} > {}: setting it to {}",
                     (uint64_t)cfg_.fuse_msg_size,
                     FUSE_DEFAULT_MAX_MAX_PAGES,
                     FUSE_DEFAULT_MAX_MAX_PAGES);
      cfg_.fuse_msg_size = std::min((uint64_t)cfg_.fuse_msg_size,
                                    (uint64_t)FUSE_DEFAULT_MAX_MAX_PAGES);
    }

  if(::_capable(conn_,FUSE_CAP_MAX_PAGES))
    {
      ::_want(conn_,FUSE_CAP_MAX_PAGES);
      fuse_cfg.max_pages = cfg_.fuse_msg_size;
      SysLog::info("requesting max pages size of {}",
                   (uint64_t)cfg_.fuse_msg_size);
    }
  else
    {
      cfg_.fuse_msg_size = FUSE_DEFAULT_MAX_PAGES_PER_REQ;
    }
}

static
void
_readahead(const fs::path &path_,
           const int       readahead_)
{
  int rv;

  rv = fs::readahead(path_,readahead_);
  if(rv == 0)
    SysLog::info("{} - readahead set to {}",path_.string(),readahead_);
  else
    SysLog::error("{} - unable to set readahead",path_.string());
}

static
void
_set_readahead_on_mount_and_branches()
{
  Branches::Ptr branches;

  if((uint64_t)cfg.readahead == 0)
    return;

  ::_readahead(cfg.mountpoint,cfg.readahead);

  branches = cfg.branches;
  for(auto const &branch : *branches)
    ::_readahead(branch.path,cfg.readahead);
}

// Spawn a thread to do this because before init returns calls to
// set the value will block leading to a deadlock. This is just
// easier.
static
void
_spawn_thread_to_set_readahead()
{
  std::thread readahead_thread(::_set_readahead_on_mount_and_branches);

  readahead_thread.detach();
}

void *
FUSE::init(fuse_conn_info *conn_)
{
  procfs::init();
  ugid::init();
  cfg.readdir.initialize();

  ::_want_if_capable(conn_,FUSE_CAP_ASYNC_DIO);
  ::_want_if_capable(conn_,FUSE_CAP_ASYNC_READ,&cfg.async_read);
  ::_want_if_capable(conn_,FUSE_CAP_ATOMIC_O_TRUNC);
  ::_want_if_capable(conn_,FUSE_CAP_BIG_WRITES);
  ::_want_if_capable(conn_,FUSE_CAP_CACHE_SYMLINKS,&cfg.cache_symlinks);
  ::_want_if_capable(conn_,FUSE_CAP_DIRECT_IO_ALLOW_MMAP,&cfg.direct_io_allow_mmap);
  ::_want_if_capable(conn_,FUSE_CAP_DONT_MASK);
  ::_want_if_capable(conn_,FUSE_CAP_EXPORT_SUPPORT,&cfg.export_support);
  ::_want_if_capable(conn_,FUSE_CAP_HANDLE_KILLPRIV,&cfg.handle_killpriv);
  ::_want_if_capable(conn_,FUSE_CAP_HANDLE_KILLPRIV_V2,&cfg.handle_killpriv_v2);
  ::_want_if_capable(conn_,FUSE_CAP_IOCTL_DIR);
  ::_want_if_capable(conn_,FUSE_CAP_PARALLEL_DIROPS);
  ::_want_if_capable(conn_,FUSE_CAP_PASSTHROUGH);
  ::_want_if_capable(conn_,FUSE_CAP_POSIX_ACL,&cfg.posix_acl);
  //  ::_want_if_capable(conn_,FUSE_CAP_READDIR_PLUS,&cfg.readdirplus);
  ::_want_if_capable(conn_,FUSE_CAP_WRITEBACK_CACHE,&cfg.cache_writeback);
  ::_want_if_capable(conn_,FUSE_CAP_ALLOW_IDMAP,&cfg.allow_idmap);
  //    ::_want_if_capable(conn_,FUSE_CAP_READDIR_PLUS_AUTO);
  ::_want_if_capable_max_pages(conn_,cfg);
  conn_->want &= ~FUSE_CAP_POSIX_LOCKS;
  conn_->want &= ~FUSE_CAP_FLOCK_LOCKS;

  ::_spawn_thread_to_set_readahead();

  if(!(conn_->capable & FUSE_CAP_PASSTHROUGH) &&
     (cfg.passthrough_io != PassthroughIO::ENUM::OFF))
    {
      SysLog::warning("passthrough enabled but not supported by kernel. disabling.");
      cfg.passthrough_io = PassthroughIO::ENUM::OFF;
    }

  if((cfg.passthrough_io != PassthroughIO::ENUM::OFF) &&
     (cfg.cache_files    == CacheFiles::ENUM::OFF))
    {
      SysLog::warning("passthrough enabled and cache.files disabled:"
                      " passthrough will not function");
    }

  if((cfg.passthrough_io  != PassthroughIO::ENUM::OFF) &&
     (cfg.cache_writeback == true))
    {
      SysLog::warning("passthrough and cache.writeback are incompatible.");
    }

  return NULL;
}
