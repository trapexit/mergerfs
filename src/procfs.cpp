#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "procfs.hpp"

#include "errno.hpp"
#include "fs_close.hpp"
#include "fs_open.hpp"
#include "fs_openat.hpp"
#include "fs_read.hpp"

#include "fmt/core.h"

#include <array>
#include <cassert>

#include <pthread.h>

static int g_PROCFS_DIR_FD = -1;
namespace procfs { int PROC_SELF_FD_FD = -1; }
static constexpr const char PROCFS_PATH[] = "/proc";
static constexpr const char PROC_SELF_FD[] = "/proc/self/fd";

// This is critical to the function of the app. abort if failed to
// open.
static
void
_open_proc_self_fd()
{
  procfs::PROC_SELF_FD_FD = fs::open(PROC_SELF_FD,O_PATH|O_DIRECTORY);
  assert(procfs::PROC_SELF_FD_FD >= 0);
}

int
procfs::init()
{
  int rv;

  if(g_PROCFS_DIR_FD >= 0)
    return 0;

  rv = fs::open(PROCFS_PATH,O_PATH|O_DIRECTORY);
  if(rv < 0)
    return rv;

  g_PROCFS_DIR_FD = rv;

#if defined(__linux__)
  ::_open_proc_self_fd();
#endif

  return 0;
}

std::string
procfs::get_name(const int tid_)
{
  int fd;
  int rv;
  std::array<char,256> commpath;
  fmt::format_to_n_result<char*> frv;

  frv = fmt::format_to_n(commpath.data(),commpath.size()-1,"{}/comm",tid_);
  frv.out[0] = '\0';

  fd = fs::openat(g_PROCFS_DIR_FD,commpath.data(),O_RDONLY);
  if(fd < 0)
    return {};

  rv = fs::read(fd,commpath.data(),commpath.size());
  if(rv < 0)
    return {};

  // Overwrite the newline with NUL
  commpath[rv-1] = '\0';

  fs::close(fd);

  return commpath.data();
}
