/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB.
*/

#include "fuse.h"
#include "fuse_opt.h"
#include "fuse_lowlevel.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/mount.h>
#include <sys/param.h>
#include <unistd.h>

int fuse_kern_mount(const char *mountpoint, struct fuse_args *args);
void fuse_kern_unmount(const char *mountpoint, int fd);

enum  {
  KEY_HELP,
  KEY_HELP_NOHEADER,
  KEY_VERSION,
};

struct helper_opts
{
  int foreground;
  int nodefault_subtype;
  char *mountpoint;
};

#define FUSE_HELPER_OPT(t, p) { t, offsetof(struct helper_opts, p), 1 }

static
const
struct fuse_opt fuse_helper_opts[] =
  {
    FUSE_HELPER_OPT("-f",	foreground),
    FUSE_HELPER_OPT("fsname=",	nodefault_subtype),
    FUSE_HELPER_OPT("subtype=",	nodefault_subtype),
    FUSE_OPT_KEY("fsname=",	FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("subtype=",	FUSE_OPT_KEY_KEEP),
    FUSE_OPT_END
  };


static
char*
_realpath_mountpoint(const char *path_,
                     char       *resolved_path_)
{
  char *rv;

  rv = realpath(path_,resolved_path_);
  // If it looks like a disconnected fuse mount try to umount it
  if((not rv) && (errno == ENOTCONN))
    {
      umount(path_);
      rv = realpath(path_,resolved_path_);
    }

  return rv;
}

static
int
fuse_helper_opt_proc(void             *data,
                     const char       *arg,
                     int               key,
                     struct fuse_args *outargs)
{
  struct helper_opts *hopts = static_cast<struct helper_opts*>(data);

  switch (key)
    {
    case FUSE_OPT_KEY_NONOPT:
      if(!hopts->mountpoint)
        {
          char mountpoint[PATH_MAX];
          if(::_realpath_mountpoint(arg, mountpoint) == NULL)
            {
              fprintf(stderr,
                      "mergerfs: bad mount point `%s': %s\n",
                      arg, strerror(errno));
              return -1;
            }
          return fuse_opt_add_opt(&hopts->mountpoint, mountpoint);
        }
      else
        {
          fprintf(stderr, "mergerfs: invalid argument `%s'\n", arg);
          return -1;
        }

    default:
      return 1;
  }
}

static int add_default_subtype(const char *progname, struct fuse_args *args)
{
  const char *basename = strrchr(progname, '/');
  if (basename == NULL)
    basename = progname;
  else if (basename[1] != '\0')
    basename++;

  std::string subtype_opt = "-osubtype=" + std::string(basename);
  return fuse_opt_add_arg(args, subtype_opt.c_str());
}

int
fuse_parse_cmdline(struct fuse_args  *args_,
                   char             **mountpoint_,
                   int               *foreground_)
{
  int res;
  struct helper_opts hopts;

  memset(&hopts, 0, sizeof(hopts));

  res = fuse_opt_parse(args_,
                       &hopts,
                       fuse_helper_opts,
                       fuse_helper_opt_proc);
  if(res == -1)
    return -1;

  if(!hopts.nodefault_subtype)
    {
      res = add_default_subtype(args_->argv[0], args_);
      if(res == -1)
        goto err;
    }

  if(mountpoint_)
    *mountpoint_ = hopts.mountpoint;
  else
    free(hopts.mountpoint);

  if(foreground_)
    *foreground_ = hopts.foreground;

  return 0;

 err:
  free(hopts.mountpoint);
  return -1;
}

int fuse_daemonize(int foreground)
{
  if (!foreground) {
    int nullfd;
    int waiter[2];
    char completed;

    if (pipe(waiter)) {
      perror("fuse_daemonize: pipe");
      return -1;
    }

    /*
     * demonize current process by forking it and killing the
     * parent.  This makes current process as a child of 'init'.
     */
    switch(fork()) {
    case -1:
      perror("fuse_daemonize: fork");
      return -1;
    case 0:
      break;
    default:
      read(waiter[0], &completed, sizeof(completed));
      _exit(0);
    }

    if (setsid() == -1) {
      perror("fuse_daemonize: setsid");
      return -1;
    }

    (void) chdir("/");

    nullfd = open("/dev/null", O_RDWR, 0);
    if (nullfd != -1) {
      (void) dup2(nullfd, 0);
      (void) dup2(nullfd, 1);
      (void) dup2(nullfd, 2);
      if (nullfd > 2)
        close(nullfd);
    }

    /* Propagate completion of daemon initializatation */
    completed = 1;
    write(waiter[1], &completed, sizeof(completed));
    close(waiter[0]);
    close(waiter[1]);
  }
  return 0;
}

static
int
fuse_mount_common(const char       *mountpoint_,
                  struct fuse_args *args_,
                  size_t           *bufsize_)
{
  int fd;
  long bufsize;
  long pagesize;

  /*
   * Make sure file descriptors 0, 1 and 2 are open, otherwise chaos
   * would ensue.
   */
  do
    {
      fd = open("/dev/null", O_RDWR);
      if(fd > 2)
        close(fd);
    } while(fd >= 0 && fd <= 2);

  fd = fuse_kern_mount(mountpoint_,args_);
  if(fd == -1)
    return -1;

  pagesize = sysconf(_SC_PAGESIZE);
  bufsize  = ((FUSE_DEFAULT_MAX_MAX_PAGES + 1) * pagesize);

  if (bufsize_)
    *bufsize_ = bufsize;

  return fd;
}

int
fuse_mount(const char       *mountpoint_,
           struct fuse_args *args_)
{
  return fuse_mount_common(mountpoint_,args_,NULL);
}

static void fuse_unmount_common(const char *mountpoint, int fd)
{
  if (mountpoint && fd != -1) {
    fuse_kern_unmount(mountpoint, fd);
  }
}

void fuse_unmount(const char *mountpoint, int fd)
{
  fuse_unmount_common(mountpoint, fd);
}

static
struct fuse*
fuse_setup_common(int argc,
                  char *argv[],
                  const struct fuse_operations *op,
                  char **mountpoint,
                  int *fd)
{
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  int fuse_fd;
  size_t bufsize;
  struct fuse *fuse;
  int foreground;
  int res;

  res = fuse_parse_cmdline(&args, mountpoint, &foreground);
  if (res == -1)
    return NULL;

  fuse_fd = fuse_mount_common(*mountpoint, &args, &bufsize);
  if (fuse_fd == -1) {
    fuse_opt_free_args(&args);
    goto err_free;
  }

  fuse = fuse_new(fuse_fd, bufsize, &args, op);
  fuse_opt_free_args(&args);
  if (fuse == NULL)
    goto err_unmount;

  res = fuse_daemonize(foreground);
  if (res == -1)
    goto err_unmount;

  res = fuse_set_signal_handlers(fuse_get_session());
  if (res == -1)
    goto err_unmount;

  if (fd)
    *fd = fuse_fd;

  return fuse;

 err_unmount:
  fuse_unmount_common(*mountpoint, fuse_fd);
 err_free:
  free(*mountpoint);
  return NULL;
}

struct fuse *fuse_setup(int argc,
                        char *argv[],
			const struct fuse_operations *op,
			char **mountpoint)
{
  return fuse_setup_common(argc,
                           argv,
                           op,
                           mountpoint,
                           NULL);
}

static void fuse_teardown_common(char *mountpoint)
{
  struct fuse_session *se = fuse_get_session();
  int fd = se ? fuse_session_fd(se) : -1;
  fuse_remove_signal_handlers(se);
  if (mountpoint && fd != -1) {
    fuse_session_clearfd(se);
    fuse_kern_unmount(mountpoint, fd);
  }
  free(mountpoint);
}

void fuse_teardown(char *mountpoint)
{
  fuse_teardown_common(mountpoint);
}

int
fuse_main(int argc,
          char *argv[],
          const struct fuse_operations *op)
{
  struct fuse *fuse;
  char *mountpoint;
  int res;

  fuse = fuse_setup_common(argc,
                           argv,
                           op,
                           &mountpoint,
                           NULL);
  if(fuse == NULL)
    return 1;

  res = fuse_loop_mt(fuse);

  fuse_teardown_common(mountpoint);
  if(res == -1)
    return 1;

  return 0;
}
