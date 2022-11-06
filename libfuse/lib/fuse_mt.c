/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB.
*/

#include "fuse_i.h"
#include "fuse_misc.h"
#include "fuse_lowlevel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

int
fuse_loop_mt(struct fuse *f)
{
  if (f == NULL)
    return -1;

  int res = fuse_start_maintenance_thread(f);
  if (res)
    return -1;

  res = fuse_session_loop_mt(fuse_get_session(f),
                             fuse_config_num_threads(f));

  fuse_stop_maintenance_thread(f);

  return res;
}
