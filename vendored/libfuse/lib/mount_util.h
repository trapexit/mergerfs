/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB.
*/

#pragma once

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#ifndef __NetBSD__
#include <mntent.h>
#endif
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/param.h>

#ifdef __NetBSD__
#define umount2(mnt, flags) unmount(mnt, (flags == 2) ? MNT_FORCE : 0)
#define mtab_needs_update(mnt) 0
#else
static int mtab_needs_update(const char *mnt)
{
  int res;
  struct stat stbuf;

  /* If mtab is within new mount, don't touch it */
  if (strncmp(mnt, _PATH_MOUNTED, strlen(mnt)) == 0 &&
      _PATH_MOUNTED[strlen(mnt)] == '/')
    return 0;

  /*
   * Skip mtab update if /etc/mtab:
   *
   *  - doesn't exist,
   *  - is a symlink,
   *  - is on a read-only filesystem.
   */
  res = lstat(_PATH_MOUNTED, &stbuf);
  if (res == -1) {
    if (errno == ENOENT)
      return 0;
  } else {
    uid_t ruid;
    int err;

    if (S_ISLNK(stbuf.st_mode))
      return 0;

    ruid = getuid();
    if (ruid != 0)
      setreuid(0, -1);

    res = access(_PATH_MOUNTED, W_OK);
    err = (res == -1) ? errno : 0;
    if (ruid != 0)
      setreuid(ruid, -1);

    if (err == EROFS)
      return 0;
  }

  return 1;
}
#endif /* __NetBSD__ */

static int add_mount(const char *progname, const char *fsname,
                     const char *mnt, const char *type, const char *opts)
{
  int res;
  int status;
  sigset_t blockmask;
  sigset_t oldmask;

  sigemptyset(&blockmask);
  sigaddset(&blockmask, SIGCHLD);
  res = sigprocmask(SIG_BLOCK, &blockmask, &oldmask);
  if (res == -1) {
    fprintf(stderr, "%s: sigprocmask: %s\n", progname, strerror(errno));
    return -1;
  }

  res = fork();
  if (res == -1) {
    fprintf(stderr, "%s: fork: %s\n", progname, strerror(errno));
    goto out_restore;
  }
  if (res == 0) {
    char *env = NULL;

    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    setuid(geteuid());
    execle("/bin/mount", "/bin/mount", "--no-canonicalize", "-i",
           "-f", "-t", type, "-o", opts, fsname, mnt, NULL, &env);
    fprintf(stderr, "%s: failed to execute /bin/mount: %s\n",
            progname, strerror(errno));
    exit(1);
  }
  res = waitpid(res, &status, 0);
  if (res == -1)
    fprintf(stderr, "%s: waitpid: %s\n", progname, strerror(errno));

  if (status != 0)
    res = -1;

 out_restore:
  sigprocmask(SIG_SETMASK, &oldmask, NULL);

  return res;
}

int fuse_mnt_add_mount(const char *progname, const char *fsname,
		       const char *mnt, const char *type, const char *opts)
{
  if (!mtab_needs_update(mnt))
    return 0;

  return add_mount(progname, fsname, mnt, type, opts);
}

static int exec_umount(const char *progname, const char *rel_mnt, int lazy)
{
  int res;
  int status;
  sigset_t blockmask;
  sigset_t oldmask;

  sigemptyset(&blockmask);
  sigaddset(&blockmask, SIGCHLD);
  res = sigprocmask(SIG_BLOCK, &blockmask, &oldmask);
  if (res == -1) {
    fprintf(stderr, "%s: sigprocmask: %s\n", progname, strerror(errno));
    return -1;
  }

  res = fork();
  if (res == -1) {
    fprintf(stderr, "%s: fork: %s\n", progname, strerror(errno));
    goto out_restore;
  }
  if (res == 0) {
    char *env = NULL;

    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    setuid(geteuid());
    if (lazy) {
      execle("/bin/umount", "/bin/umount", "-i", rel_mnt,
             "-l", NULL, &env);
    } else {
      execle("/bin/umount", "/bin/umount", "-i", rel_mnt,
             NULL, &env);
    }
    fprintf(stderr, "%s: failed to execute /bin/umount: %s\n",
            progname, strerror(errno));
    exit(1);
  }
  res = waitpid(res, &status, 0);
  if (res == -1)
    fprintf(stderr, "%s: waitpid: %s\n", progname, strerror(errno));

  if (status != 0) {
    res = -1;
  }

 out_restore:
  sigprocmask(SIG_SETMASK, &oldmask, NULL);
  return res;

}

int fuse_mnt_umount(const char *progname, const char *abs_mnt,
		    const char *rel_mnt, int lazy)
{
  int res;

  if (!mtab_needs_update(abs_mnt)) {
    res = umount2(rel_mnt, lazy ? MNT_DETACH : 0);
    if (res == -1)
      fprintf(stderr, "%s: failed to unmount %s: %s\n",
              progname, abs_mnt, strerror(errno));
    return res;
  }

  return exec_umount(progname, rel_mnt, lazy);
}

static int remove_mount(const char *progname, const char *mnt)
{
  int res;
  int status;
  sigset_t blockmask;
  sigset_t oldmask;

  sigemptyset(&blockmask);
  sigaddset(&blockmask, SIGCHLD);
  res = sigprocmask(SIG_BLOCK, &blockmask, &oldmask);
  if (res == -1) {
    fprintf(stderr, "%s: sigprocmask: %s\n", progname, strerror(errno));
    return -1;
  }

  res = fork();
  if (res == -1) {
    fprintf(stderr, "%s: fork: %s\n", progname, strerror(errno));
    goto out_restore;
  }
  if (res == 0) {
    char *env = NULL;

    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    setuid(geteuid());
    execle("/bin/umount", "/bin/umount", "--no-canonicalize", "-i",
           "--fake", mnt, NULL, &env);
    fprintf(stderr, "%s: failed to execute /bin/umount: %s\n",
            progname, strerror(errno));
    exit(1);
  }
  res = waitpid(res, &status, 0);
  if (res == -1)
    fprintf(stderr, "%s: waitpid: %s\n", progname, strerror(errno));

  if (status != 0)
    res = -1;

 out_restore:
  sigprocmask(SIG_SETMASK, &oldmask, NULL);
  return res;
}

int fuse_mnt_remove_mount(const char *progname, const char *mnt)
{
  if (!mtab_needs_update(mnt))
    return 0;

  return remove_mount(progname, mnt);
}

char *fuse_mnt_resolve_path(const char *progname, const char *orig)
{
  char buf[PATH_MAX];

  if (!orig[0]) {
    fprintf(stderr, "%s: invalid mountpoint '%s'\n", progname,
            orig);
    return NULL;
  }

  std::string copy = orig;
  std::string lastcomp;
  std::string toresolv;

  while (!copy.empty() && copy.back() == '/')
    copy.pop_back();

  if (!copy.empty() && copy.find('/') != std::string::npos) {
    size_t last_slash = copy.find_last_of('/');
    lastcomp = copy.substr(last_slash + 1);
    toresolv = copy.substr(0, last_slash);
    if(toresolv.empty())
      toresolv = "/";

    if (lastcomp == "." || lastcomp == "..") {
      lastcomp.clear();
      toresolv = copy;
    }
  } else if (!copy.empty()) {
    lastcomp = copy;
    toresolv = ".";
  } else {
    toresolv = "/";
  }

  if (realpath(toresolv.c_str(), buf) == NULL) {
    fprintf(stderr, "%s: bad mount point %s: %s\n", progname, orig,
            strerror(errno));
    return NULL;
  }

  std::string dst;
  if (lastcomp.empty())
    dst = buf;
  else if (buf[strlen(buf) - 1] == '/')
    dst = std::string(buf) + lastcomp;
  else
    dst = std::string(buf) + "/" + lastcomp;

  return strdup(dst.c_str());
}

int fuse_mnt_check_fuseblk(void)
{
  char buf[256];
  FILE *f = fopen("/proc/filesystems", "r");
  if (!f)
    return 1;

  while (fgets(buf, sizeof(buf), f))
    if (strstr(buf, "fuseblk\n")) {
      fclose(f);
      return 1;
    }

  fclose(f);
  return 0;
}
