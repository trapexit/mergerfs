#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "procfs_get_name.hpp"

#include "errno.hpp"
#include "fs_close.hpp"
#include "fs_open.hpp"
#include "fs_openat.hpp"
#include "fs_read.hpp"

#include <pthread.h>

static int g_PROCFS_DIR_FD = -1;
const char PROCFS_PATH[] = "/proc";

int
procfs::init()
{
  if(g_PROCFS_DIR_FD != -1)
    return 0;

  g_PROCFS_DIR_FD = fs::open(PROCFS_PATH,O_PATH|O_DIRECTORY);
  if(g_PROCFS_DIR_FD == -1)
    return -errno;

  return 0;
}

std::string
procfs::get_name(const int tid_)
{
  int fd;
  int rv;
  char commpath[256];

  snprintf(commpath,sizeof(commpath),"%d/comm",tid_);

  fd = fs::openat(g_PROCFS_DIR_FD,commpath,O_RDONLY);
  if(fd < 0)
    return {};

  rv = fs::read(fd,commpath,sizeof(commpath));
  if(rv == -1)
    return {};

  // Overwrite the newline with NUL
  commpath[rv-1] = '\0';

  fs::close(fd);

  return commpath;
}
