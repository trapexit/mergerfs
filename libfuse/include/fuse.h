/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB.
*/

#ifndef _FUSE_H_
#define _FUSE_H_

#include "extern_c.h"
#include "fuse_common.h"
#include "fuse_kernel.h"
#include "fuse_req_ctx.h"
#include "fuse_conn_info.hpp"

#include <fcntl.h>
#include <time.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/uio.h>
#include <stdint.h>

EXTERN_C_BEGIN

/* ----------------------------------------------------------- *
 * Basic FUSE API					       *
 * ----------------------------------------------------------- */

struct fuse_dirents_t;
typedef struct fuse_dirents_t fuse_dirents_t;

/**
 * The file system operations:
 *
 * Most of these should work very similarly to the well known UNIX
 * file system operations.  A major exception is that instead of
 * returning an error in 'errno', the operation should return the
 * negated error value (-errno) directly.
 *
 * All methods are optional, but some are essential for a useful
 * filesystem (e.g. getattr).  Open, flush, release, fsync, opendir,
 * releasedir, fsyncdir, access, create, ftruncate, fgetattr, lock,
 * init and destroy are special purpose methods, without which a full
 * featured filesystem can still be implemented.
 *
 * Almost all operations take a path which can be of any length.
 *
 * Changed in fuse 2.8.0 (regardless of API version)
 * Previously, paths were limited to a length of PATH_MAX.
 *
 * See http://fuse.sourceforge.net/wiki/ for more information.  There
 * is also a snapshot of the relevant wiki pages in the doc/ folder.
 */
struct fuse_operations
{
  int (*getattr)(const fuse_req_ctx_t *,
                 const char *,
                 struct stat *,
                 fuse_timeouts_t *);
  int (*readlink)(const fuse_req_ctx_t *,
                  const char *,
                  char *,
                  size_t);
  int (*mknod)(const fuse_req_ctx_t *,
               const char *,
               mode_t,
               dev_t);
  int (*mkdir)(const fuse_req_ctx_t *,
               const char *,
               mode_t);
  int (*unlink)(const fuse_req_ctx_t *,
                const char *);
  int (*rmdir)(const fuse_req_ctx_t *,
               const char *);
  int (*symlink)(const fuse_req_ctx_t *,
                 const char *,
                 const char *,
                 struct stat *,
                 fuse_timeouts_t *);
  int (*rename)(const fuse_req_ctx_t *,
                const char *,
                const char *);
  int (*link)(const fuse_req_ctx_t *,
              const char *,
              const char *,
              struct stat *,
              fuse_timeouts_t *);
  int (*chmod)(const fuse_req_ctx_t *,
               const char *,
               mode_t);
  int (*fchmod)(const fuse_req_ctx_t *,
                const uint64_t,
                const mode_t);
  int (*chown)(const fuse_req_ctx_t *,
               const char *,
               uid_t,
               gid_t);
  int (*fchown)(const fuse_req_ctx_t *,
                const uint64_t,
                const uid_t,
                const gid_t);
  int (*truncate)(const fuse_req_ctx_t *,
                  const char *,
                  off_t);
  int (*open)(const fuse_req_ctx_t *,
              const char *,
              fuse_file_info_t *);
  int (*statfs)(const fuse_req_ctx_t *,
                const char *,
                struct statvfs *);
  int (*flush)(const fuse_req_ctx_t *,
               const fuse_file_info_t *);
  int (*release)(const fuse_req_ctx_t *,
                 const fuse_file_info_t *);
  int (*fsync)(const fuse_req_ctx_t *,
               const uint64_t,
               int);
  int (*setxattr)(const fuse_req_ctx_t *,
                  const char *,
                  const char *,
                  const char *,
                  size_t,
                  int);
  int (*getxattr)(const fuse_req_ctx_t *,
                  const char *,
                  const char *,
                  char *,
                  size_t);
  int (*listxattr)(const fuse_req_ctx_t *,
                   const char *,
                   char *,
                   size_t);
  int (*removexattr)(const fuse_req_ctx_t *,
                     const char *,
                     const char *);
  int (*opendir)(const fuse_req_ctx_t *,
                 const char *,
                 fuse_file_info_t *);
  int (*readdir)(const fuse_req_ctx_t *,
                 const fuse_file_info_t *,
                 fuse_dirents_t *);
  int (*readdir_plus)(const fuse_req_ctx_t *,
                      const fuse_file_info_t *,
                      fuse_dirents_t *);
  int (*releasedir)(const fuse_req_ctx_t *,
                    const fuse_file_info_t *);
  int (*fsyncdir)(const fuse_req_ctx_t *,
                  const fuse_file_info_t *,
                  int);
  void *(*init)(struct fuse_conn_info_t *conn);
  void (*destroy)(void);
  int (*access)(const fuse_req_ctx_t *,
                const char *,
                int);
  int (*create)(const fuse_req_ctx_t *,
                const char *,
                mode_t,
                fuse_file_info_t *);
  int (*ftruncate)(const fuse_req_ctx_t *,
                   const uint64_t,
                   off_t);
  int (*fgetattr)(const fuse_req_ctx_t *,
                  const uint64_t,
                  struct stat *,
                  fuse_timeouts_t *);
  int (*lock)(const fuse_req_ctx_t *,
              const fuse_file_info_t *,
              int cmd,
              struct flock *);
  int (*utimens)(const fuse_req_ctx_t *,
                 const char *,
                 const struct timespec tv[2]);
  int (*futimens)(const fuse_req_ctx_t *,
                  const uint64_t fh,
                  const struct timespec tv[2]);
  int (*bmap)(const fuse_req_ctx_t *,
              const char *,
              size_t blocksize,
              uint64_t *idx);
  int (*ioctl)(const fuse_req_ctx_t   *ctx,
               const fuse_file_info_t *ffi,
               unsigned long           cmd,
               void                   *arg,
               unsigned int            flags,
               void                   *data,
               uint32_t               *out_bufsz);
  int (*write)(const fuse_req_ctx_t *,
               const fuse_file_info_t *ffi,
               const char             *data,
               size_t                  size,
               off_t                   off);
  int (*read)(const fuse_req_ctx_t *,
              const fuse_file_info_t *ffi,
              char                   *buf,
              size_t                  size,
              off_t                   off);
  int (*flock)(const fuse_req_ctx_t *,
               const fuse_file_info_t *,
               int op);
  int (*fallocate)(const fuse_req_ctx_t *,
                   const uint64_t,
                   int,
                   off_t,
                   off_t);
  ssize_t (*copy_file_range)(const fuse_req_ctx_t *,
                             const fuse_file_info_t *src_fi,
                             off_t                   src_off,
                             const fuse_file_info_t *dst_fi,
                             off_t                   dst_off,
                             const size_t            size,
                             const unsigned int      flags);
  ssize_t (*setupmapping)(const fuse_req_ctx_t *,
                          uint64_t *fh_,
                          uint64_t  foffset_,
                          uint64_t  len_,
                          uint64_t  flags_,
                          uint64_t  moffset_);
  int (*removemapping)(const fuse_req_ctx_t *);
  int (*syncfs)(const fuse_req_ctx_t *);
  int (*tmpfile)(const fuse_req_ctx_t *,
                 const char *,
                 mode_t,
                 fuse_file_info_t *);
  int (*statx)(const fuse_req_ctx_t *,
               const char        *fusepath,
               const uint32_t     flags,
               const uint32_t     mask,
               struct fuse_statx *st,
               fuse_timeouts_t   *timeout);
  int (*statx_fh)(const fuse_req_ctx_t *,
                  const uint64_t     fh,
                  const uint32_t     flags,
                  const uint32_t     mask,
                  struct fuse_statx *st,
                  fuse_timeouts_t   *timeout);
};

/**
 * Main function of FUSE.
 *
 * This is for the lazy.  This is all that has to be called from the
 * main() function.
 *
 * This function does the following:
 *   - parses command line options (-d -s and -h)
 *   - passes relevant mount options to the fuse_mount()
 *   - installs signal handlers for INT, HUP, TERM and PIPE
 *   - registers an exit handler to unmount the filesystem on program exit
 *   - creates a fuse handle
 *   - registers the operations
 *   - calls either the single-threaded or the multi-threaded event loop
 *
 * Note: this is currently implemented as a macro.
 *
 * @param argc the argument counter passed to the main() function
 * @param argv the argument vector passed to the main() function
 * @param op the file system operation
 * @return 0 on success, nonzero on failure
 */

/* ----------------------------------------------------------- *
 * More detailed API					       *
 * ----------------------------------------------------------- */

/**
 * Create a new FUSE filesystem.
 *
 * @param ch the communication channel
 * @param args argument vector
 * @param op the filesystem operations
 * @param op_size the size of the fuse_operations structure
 * @return the created FUSE handle
 */
struct fuse *fuse_new(struct fuse_chan *ch,
                      struct fuse_args *args,
                      const struct fuse_operations *op);

/**
 * Destroy the FUSE handle.
 *
 * The communication channel attached to the handle is also destroyed.
 *
 * NOTE: This function does not unmount the filesystem.	 If this is
 * needed, call fuse_unmount() before calling this function.
 *
 * @param f the FUSE handle
 */
void fuse_destroy();

/**
 * Exit from event loop
 *
 * @param f the FUSE handle
 */
void fuse_exit(struct fuse *f);

int fuse_config_read_thread_count(const struct fuse *f);
int fuse_config_process_thread_count(const struct fuse *f);
int fuse_config_process_thread_queue_depth(const struct fuse *f);
const char* fuse_config_pin_threads(const struct fuse *f);

/**
 * FUSE event loop with multiple threads
 *
 * Requests from the kernel are processed, and the appropriate
 * operations are called.  Request are processed in parallel by
 * distributing them between multiple threads.
 *
 * Calling this function requires the pthreads library to be linked to
 * the application.
 *
 * @param f the FUSE handle
 * @return 0 if no error occurred, -1 otherwise
 */
int fuse_loop_mt(struct fuse *f);

/**
 * Get the current context
 *
 * The context is only valid for the duration of a filesystem
 * operation, and thus must not be stored and used later.
 *
 * @return the context
 */
struct fuse_context *fuse_get_context(void);

/**
 * Check if the current request has already been interrupted
 *
 * @return 1 if the request has been interrupted, 0 otherwise
 */
int fuse_interrupted(void);

/**
 * Obsolete, doesn't do anything
 *
 * @return -EINVAL
 */
int fuse_invalidate(struct fuse *f, const char *path);

/* Deprecated, don't use */
int fuse_is_lib_option(const char *opt);

/**
 * The real main function
 *
 * Do not call this directly, use fuse_main()
 */
int fuse_main(int argc,
              char *argv[],
              const struct fuse_operations *op);

void fuse_populate_maintenance_thread(struct fuse *fuse);

int  fuse_log_metrics_get(void);
void fuse_log_metrics_set(int enabled);


/**
 * Iterate over cache removing stale entries
 * use in conjunction with "-oremember"
 *
 * NOTE: This is already done for the standard sessions
 *
 * @param fuse struct fuse pointer for fuse instance
 * @return the number of seconds until the next cleanup
 */
int fuse_clean_cache(struct fuse *fuse);

/*
 * Stacking API
 */

/*
 * These functions call the relevant filesystem operation, and return
 * the result.
 *
 * If the operation is not defined, they return -ENOSYS, with the
 * exception of fuse_fs_open, fuse_fs_release, fuse_fs_opendir,
 * fuse_fs_releasedir and fuse_fs_statfs, which return 0.
 */

/* ----------------------------------------------------------- *
 * Advanced API for event handling, don't worry about this...  *
 * ----------------------------------------------------------- */

/* NOTE: the following functions are deprecated, and will be removed
   from the 3.0 API.  Use the lowlevel session functions instead */

/** This is the part of fuse_main() before the event loop */
struct fuse *fuse_setup(int argc,
                        char *argv[],
                        const struct fuse_operations *op,
                        char **mountpoint);

/** This is the part of fuse_main() after the event loop */
void fuse_teardown(char *mountpoint);

/** Return the exited flag, which indicates if fuse_exit() has been
    called */
int fuse_exited();

/** This function is obsolete and implemented as a no-op */
void fuse_set_getcontext_func(struct fuse_context *(*func)(void));

/** Get session from fuse object */
struct fuse_session *fuse_get_session();

void fuse_gc1();
void fuse_gc();
void fuse_invalidate_all_nodes();

int fuse_passthrough_open(const int fd);
int fuse_passthrough_close(const int backing_id);

EXTERN_C_END

#endif /* _FUSE_H_ */
