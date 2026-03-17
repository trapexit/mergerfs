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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "procfs.hpp"

#include "fatal.hpp"
#include "fs_close.hpp"
#include "fs_open.hpp"
#include "fs_openat.hpp"
#include "fs_read.hpp"

#include "fmt/core.h"
#include "scope_guard.hpp"

#include <array>
#include <cstring>

static int g_PROCFS_DIR_FD = -1;
namespace procfs { int PROC_SELF_FD_FD = -1; }
static constexpr const char PROCFS_PATH[] = "/proc";

void
procfs::init()
{
  if(g_PROCFS_DIR_FD >= 0)
    return;

  g_PROCFS_DIR_FD = fs::open(PROCFS_PATH,O_PATH|O_DIRECTORY);
  if(g_PROCFS_DIR_FD < 0)
    fatal::abort("failed to open {}: {}",PROCFS_PATH,strerror(-g_PROCFS_DIR_FD));

#if defined(__linux__)
  static constexpr const char PROC_SELF_FD[] = "/proc/self/fd";

  procfs::PROC_SELF_FD_FD = fs::open(PROC_SELF_FD,O_PATH|O_DIRECTORY);
  if(procfs::PROC_SELF_FD_FD < 0)
    fatal::abort("failed to open /proc/self/fd: {}",strerror(-procfs::PROC_SELF_FD_FD));
#endif
}

void
procfs::shutdown()
{
  if(g_PROCFS_DIR_FD >= 0)
    {
      fs::close(g_PROCFS_DIR_FD);
      g_PROCFS_DIR_FD = -1;
    }
#if defined(__linux__)
  if(procfs::PROC_SELF_FD_FD >= 0)
    {
      fs::close(procfs::PROC_SELF_FD_FD);
      procfs::PROC_SELF_FD_FD = -1;
    }
#endif
}

std::string
procfs::get_name(const int tid_)
{
  int fd;
  int rv;
  std::array<char,256> commpath;
  fmt::format_to_n_result<char*> frv;

  if(g_PROCFS_DIR_FD < 0)
    fatal::abort("procfs::get_name called before procfs::init()");

  // Reserve 1 byte for the NUL terminator that format_to_n does not write.
  frv = fmt::format_to_n(commpath.data(),commpath.size()-1,"{}/comm",tid_);
  frv.out[0] = '\0';

  fd = fs::openat(g_PROCFS_DIR_FD,commpath.data(),O_RDONLY);
  if(fd < 0)
    return {};
  DEFER { fs::close(fd); };

  rv = fs::read(fd,commpath.data(),commpath.size()-1);
  if(rv <= 0)
    return {};

  if(commpath[rv-1] == '\n')
    commpath[rv-1] = '\0';
  else
    commpath[rv] = '\0';

  return commpath.data();
}
