/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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
#include "pipe_max_size.hpp"
#include "procfs.hpp"
#include "state.hpp"
#include "syslog.hpp"

#include "fs_path.hpp"
#include "fs_exists.hpp"

#include "fuse.h"

#include <algorithm>
#include <fstream>
#include <thread>

#include <unistd.h>


static
void
_want(fuse_conn_info_t *conn_,
      const int         flag_)
{
  conn_->want |= flag_;
}

static
bool
_capable(fuse_conn_info_t *conn_,
         const int         flag_)
{
  return !!(conn_->capable & flag_);
}

static
void
_want_if_capable(fuse_conn_info_t *conn_,
                 const int         flag_)
{
  if(::_capable(conn_,flag_))
    ::_want(conn_,flag_);
}

static
void
_want_if_capable(fuse_conn_info_t *conn_,
                 const int         flag_,
                 ConfigBOOL       *want_)
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

/* Single place that decides what "splice enabled/disabled" means:
   cfg_.splice, all three fuse_cfg.splice_* fields, and the three
   FUSE_CAP_SPLICE_* bits in conn_->want always move together. Used
   both when the kernel/administrator decision is first applied
   (FUSE::init) and when INIT-time pipe negotiation later retracts it
   (_negotiate_splice_pipe_capacity below) - a single call site means
   a future fourth splice-related flag only needs updating here. */
static
void
_set_splice_enabled(fuse_conn_info_t *conn_,
                     Config           &cfg_,
                     const bool        enabled_)
{
  cfg_.splice           = enabled_;
  fuse_cfg.splice_read  = enabled_;
  fuse_cfg.splice_write = enabled_;
  fuse_cfg.splice_move  = enabled_;

  if(enabled_)
    {
      ::_want(conn_,FUSE_CAP_SPLICE_READ);
      ::_want(conn_,FUSE_CAP_SPLICE_WRITE);
      ::_want(conn_,FUSE_CAP_SPLICE_MOVE);
    }
  else
    {
      conn_->want &= ~(FUSE_CAP_SPLICE_READ  |
                        FUSE_CAP_SPLICE_WRITE |
                        FUSE_CAP_SPLICE_MOVE);
    }
}

static
void
_negotiate_splice_pipe_capacity(fuse_conn_info_t *conn_,
                                 Config           &cfg_)
{
  if(!fuse_cfg.splice_read && !fuse_cfg.splice_write)
    return;

  /* Splice receive needs one whole request to fit in the per-msgbuf
     pipe. The pipe cap is min(g_bufsize, /proc/sys/fs/pipe-max-size),
     so max_pages must satisfy
     (max_pages + 2) * pagesize <= effective_pipe_max (the +2 covers
     the 16B fuse_in_header, the fuse_write_in the receive path
     stages before the payload, and the 16B fuse_out_header on
     replies).

     Modeled on the max_pages_limit write above: when the requested
     fuse_msg_size does not fit the current sysctl, attempt to raise
     it (root only, best effort, non-fatal), then re-read and clamp
     downward if the write didn't take. */
  u64 pagesize = (u64)::sysconf(_SC_PAGESIZE);

  u32 pipe_max = 1048576;
  pipe_max_size::read(pipe_max);

  u64 needed = ((u64)cfg_.fuse_msg_size + 2) * pagesize;
  if(needed > pipe_max)
    {
      if(pipe_max_size::write(needed))
        {
          SysLog::info("{} raised to {} to fit fuse_msg_size",
                       "/proc/sys/fs/pipe-max-size",
                       needed);
          if(!pipe_max_size::read(pipe_max))
            {
              SysLog::info("pipe-max-size re-read failed after"
                           " raise; assuming requested {}",
                           needed);
              /* `needed` is u64; pipe_max is u32. Clamp rather
                 than truncate - a silent mod-2^32 wrap here
                 would feed a tiny bogus value into the
                 fused_pages math just below and could disable
                 splice entirely (or wrongly clamp fuse_msg_size)
                 even though the sysctl write above just
                 succeeded at the real, large value. */
              pipe_max = (needed > UINT32_MAX) ? UINT32_MAX : (u32)needed;
            }
        }
      else
        {
          SysLog::info("splice receive on: unable to raise {}"
                       " (not root); keeping {}",
                       "/proc/sys/fs/pipe-max-size",
                       pipe_max);
        }
    }

  /* fused math: (pipe_max/pages)-2 underflows u32 when pipe_max < 3
     pages (reachable via a sysctl set to a tiny value); with exactly
     3 pages the clamp yields fused_cap = 1 page - the smallest
     workable splice receive. At 2 pages fused_cap would be 0,
     negotiating a degenerate max_pages=0 mount, so splice must be
     disabled entirely rather than negotiate sizes we can never
     honor. */
  u64 fused_pages = (pipe_max / pagesize);
  if(fused_pages < 3)
    {
      SysLog::warning("pipe-max-size {} too small for splice"
                      " (need >= 3 pages); disabling splice"
                      " read/write/move for this mount",
                      pipe_max);
      /* Runtime disable: fuse_cfg is what the live paths check
         (receive re-checks post-INIT, write/reply gates read it
         directly). Also flip cfg_.splice (the single mergerfs-level
         knob) so the startup Config dump and the splice control-file
         xattr report the true, now-disabled state instead of the
         stale pre-negotiation value. Also retract the FUSE_CAP_SPLICE_*
         bits already OR'd into conn_->want by the caller's earlier
         _want() calls (FUSE::init, before _want_if_capable_max_pages
         runs) - otherwise do_init's outargflags (fuse_lowlevel.cpp)
         would still advertise splice support to the kernel in the
         FUSE_INIT reply even though every internal path just got
         switched off. */
      _set_splice_enabled(conn_,cfg_,false);
      return;
    }

  u32 fused_cap = (u32)(fused_pages - 2);
  if(cfg_.fuse_msg_size > fused_cap)
    {
      SysLog::info("splice receive on: clamping max pages {} -> {}",
                   (u64)cfg_.fuse_msg_size,
                   (u64)fused_cap);
      cfg_.fuse_msg_size = fused_cap;
    }
}

static
void
_want_if_capable_max_pages(fuse_conn_info_t *conn_,
                           Config           &cfg_)
{
  std::fstream f;
  u64 max_pages_limit;

  if(fs::exists(MAX_PAGES_LIMIT_FILEPATH))
    {
      if(cfg_.fuse_msg_size > MAX_FUSE_MSG_SIZE)
        SysLog::info("fuse_msg_size > {}: setting it to {}",
                     MAX_FUSE_MSG_SIZE,
                     MAX_FUSE_MSG_SIZE);
      cfg_.fuse_msg_size = std::min((u64)cfg_.fuse_msg_size,
                                    (u64)MAX_FUSE_MSG_SIZE);

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
              f << (u64)cfg_.fuse_msg_size;
              f.flush();
              SysLog::info("{} changed to {}",
                           MAX_PAGES_LIMIT_FILEPATH,
                           (u64)cfg_.fuse_msg_size);
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
                     (u64)cfg_.fuse_msg_size,
                     FUSE_DEFAULT_MAX_MAX_PAGES,
                     FUSE_DEFAULT_MAX_MAX_PAGES);
      cfg_.fuse_msg_size = std::min((u64)cfg_.fuse_msg_size,
                                    (u64)FUSE_DEFAULT_MAX_MAX_PAGES);
    }

  if(::_capable(conn_,FUSE_CAP_MAX_PAGES))
    {
      ::_want(conn_,FUSE_CAP_MAX_PAGES);

      _negotiate_splice_pipe_capacity(conn_,cfg_);

      fuse_cfg.max_pages = cfg_.fuse_msg_size;
      SysLog::info("requesting max pages size of {}",
                   (u64)cfg_.fuse_msg_size);
    }
  else
    {
      SysLog::info("kernel lacks FUSE_MAX_PAGES: capping fuse_msg_size"
                   " at default {} pages",
                   (u64)FUSE_DEFAULT_MAX_PAGES_PER_REQ);
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

  if((u64)cfg.readahead == 0)
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
FUSE::init(fuse_conn_info_t *conn_)
{
  procfs::init();
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
  /* Single splice knob drives all three internal axes: only keep
     cfg.splice on when the kernel supports ALL THREE FUSE_CAP_SPLICE_*
     bits. Each _capable() check must be independent here - chaining
     three calls through the shared ConfigBOOL overload above (as a
     prior version of this code did) lets an unsupported flag checked
     first latch cfg.splice=false and suppress _want() for a later
     flag the kernel DOES support. fuse_cfg.splice_read/write/move are
     resynced immediately below (they were previously only set once
     from the pre-INIT option value in option_parser.cpp and never
     updated here), so the receive/write/reply hot paths - and
     _want_if_capable_max_pages just below, which reads them - see the
     post-negotiation decision instead of stale unfulfilled intent. */
  {
    bool splice_capable = (::_capable(conn_,FUSE_CAP_SPLICE_READ)  &&
                            ::_capable(conn_,FUSE_CAP_SPLICE_WRITE) &&
                            ::_capable(conn_,FUSE_CAP_SPLICE_MOVE));

    if(bool(cfg.splice) && !splice_capable)
      SysLog::warning("kernel lacks one or more of"
                      " FUSE_CAP_SPLICE_READ/WRITE/MOVE: disabling splice"
                      " for this mount");

    _set_splice_enabled(conn_,cfg,bool(cfg.splice) && splice_capable);
  }
  //    ::_want_if_capable(conn_,FUSE_CAP_READDIR_PLUS_AUTO);
  ::_want_if_capable_max_pages(conn_,cfg);

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

  SysLog::info("Config:");
  for(const auto &kv : cfg.get_map())
    {
      if(not kv.second->display)
        continue;
      SysLog::info("{}={}",kv.first,kv.second->to_string());
    }

  return NULL;
}
