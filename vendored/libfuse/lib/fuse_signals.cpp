/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#include "fuse_lowlevel.h"

#include <atomic>

#include <stdio.h>
#include <string.h>
#include <signal.h>

static std::atomic<fuse_session*> g_fuse_instance{nullptr};

static
void
exit_handler(int sig)
{
  (void)sig;
  struct fuse_session *instance;

  instance = g_fuse_instance.load(std::memory_order_acquire);
  if(instance)
    fuse_session_exit(instance);
}

static int set_one_signal_handler(int sig, void (*handler)(int), int remove)
{
  struct sigaction sa;
  struct sigaction old_sa;

  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = remove ? SIG_DFL : handler;
  sigemptyset(&(sa.sa_mask));
  sa.sa_flags = 0;

  if (sigaction(sig, NULL, &old_sa) == -1) {
    perror("fuse: cannot get old signal handler");
    return -1;
  }

  if (old_sa.sa_handler == (remove ? handler : SIG_DFL) &&
      sigaction(sig, &sa, NULL) == -1) {
    perror("fuse: cannot set signal handler");
    return -1;
  }
  return 0;
}

int fuse_set_signal_handlers(struct fuse_session *se)
{
  if((set_one_signal_handler(SIGINT, exit_handler, 0)  == -1) ||
     (set_one_signal_handler(SIGTERM, exit_handler, 0) == -1) ||
     (set_one_signal_handler(SIGQUIT, exit_handler, 0) == -1) ||
     (set_one_signal_handler(SIGPIPE, SIG_IGN, 0)      == -1))
    return -1;

  g_fuse_instance.store(se,std::memory_order_release);

  return 0;
}

void fuse_remove_signal_handlers(struct fuse_session *se)
{
  set_one_signal_handler(SIGINT, exit_handler, 1);
  set_one_signal_handler(SIGTERM, exit_handler, 1);
  set_one_signal_handler(SIGQUIT, exit_handler, 1);
  set_one_signal_handler(SIGPIPE, SIG_IGN, 1);

  if(g_fuse_instance.load(std::memory_order_acquire) != se)
    fprintf(stderr,"mergerfs: fuse_remove_signal_handlers: unknown session\n");
  else
    g_fuse_instance.store(nullptr,std::memory_order_release);
}
