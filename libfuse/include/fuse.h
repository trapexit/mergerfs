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

/** Handle for a FUSE filesystem */
struct fuse;

/** Structure containing a raw command */
struct fuse_cmd;

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
  /** Get file attributes.
   *
   * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
   * ignored.	 The 'st_ino' field is ignored except if the 'use_ino'
   * mount option is given.
   */
  int (*getattr) (const char *, struct stat *, fuse_timeouts_t *);

  /** Read the target of a symbolic link
   *
   * The buffer should be filled with a null terminated string.  The
   * buffer size argument includes the space for the terminating
   * null character.	If the linkname is too long to fit in the
   * buffer, it should be truncated.	The return value should be 0
   * for success.
   */
  int (*readlink) (const char *, char *, size_t);

  /** Create a file node
   *
   * This is called for creation of all non-directory, non-symlink
   * nodes.  If the filesystem defines a create() method, then for
   * regular files that will be called instead.
   */
  int (*mknod) (const char *, mode_t, dev_t);

  /** Create a directory
   *
   * Note that the mode argument may not have the type specification
   * bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
   * correct directory type bits use  mode|S_IFDIR
   * */
  int (*mkdir) (const char *, mode_t);

  /** Remove a file */
  int (*unlink) (const char *);

  /** Remove a directory */
  int (*rmdir) (const char *);

  /** Create a symbolic link */
  int (*symlink) (const char *, const char *, struct stat *, fuse_timeouts_t *);

  /** Rename a file */
  int (*rename) (const char *, const char *);

  /** Create a hard link to a file */
  int (*link) (const char *, const char *, struct stat *, fuse_timeouts_t *);

  /** Change the permission bits of a file */
  int (*chmod) (const char *, mode_t);
  int (*fchmod)(const uint64_t, const mode_t);

  /** Change the owner and group of a file */
  int (*chown) (const char *, uid_t, gid_t);
  int (*fchown)(const uint64_t, const uid_t, const gid_t);

  /** Change the size of a file */
  int (*truncate) (const char *, off_t);

  /** File open operation
   *
   * No creation (O_CREAT, O_EXCL) and by default also no
   * truncation (O_TRUNC) flags will be passed to open(). If an
   * application specifies O_TRUNC, fuse first calls truncate()
   * and then open(). Only if 'atomic_o_trunc' has been
   * specified and kernel version is 2.6.24 or later, O_TRUNC is
   * passed on to open.
   *
   * Unless the 'default_permissions' mount option is given,
   * open should check if the operation is permitted for the
   * given flags. Optionally open may also return an arbitrary
   * filehandle in the fuse_file_info structure, which will be
   * passed to all file operations.
   *
   * Changed in version 2.2
   */
  int (*open) (const char *, fuse_file_info_t *);

  /** Get file system statistics
   *
   * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
   *
   * Replaced 'struct statfs' parameter with 'struct statvfs' in
   * version 2.5
   */
  int (*statfs) (const char *, struct statvfs *);

  /** Possibly flush cached data
   *
   * BIG NOTE: This is not equivalent to fsync().  It's not a
   * request to sync dirty data.
   *
   * Flush is called on each close() of a file descriptor.  So if a
   * filesystem wants to return write errors in close() and the file
   * has cached dirty data, this is a good place to write back data
   * and return any errors.  Since many applications ignore close()
   * errors this is not always useful.
   *
   * NOTE: The flush() method may be called more than once for each
   * open().	This happens if more than one file descriptor refers
   * to an opened file due to dup(), dup2() or fork() calls.	It is
   * not possible to determine if a flush is final, so each flush
   * should be treated equally.  Multiple write-flush sequences are
   * relatively rare, so this shouldn't be a problem.
   *
   * Filesystems shouldn't assume that flush will always be called
   * after some writes, or that if will be called at all.
   *
   * Changed in version 2.2
   */
  int (*flush) (const fuse_file_info_t *);

  /** Release an open file
   *
   * Release is called when there are no more references to an open
   * file: all file descriptors are closed and all memory mappings
   * are unmapped.
   *
   * For every open() call there will be exactly one release() call
   * with the same flags and file descriptor.	 It is possible to
   * have a file opened more than once, in which case only the last
   * release will mean, that no more reads/writes will happen on the
   * file.  The return value of release is ignored.
   *
   * Changed in version 2.2
   */
  int (*release) (const fuse_file_info_t *);

  /** Synchronize file contents
   *
   * If the datasync parameter is non-zero, then only the user data
   * should be flushed, not the meta data.
   *
   * Changed in version 2.2
   */
  int (*fsync) (const uint64_t, int);

  /** Set extended attributes */
  int (*setxattr) (const char *, const char *, const char *, size_t, int);

  /** Get extended attributes */
  int (*getxattr) (const char *, const char *, char *, size_t);

  /** List extended attributes */
  int (*listxattr) (const char *, char *, size_t);

  /** Remove extended attributes */
  int (*removexattr) (const char *, const char *);

  /** Open directory
   *
   * Unless the 'default_permissions' mount option is given,
   * this method should check if opendir is permitted for this
   * directory. Optionally opendir may also return an arbitrary
   * filehandle in the fuse_file_info structure, which will be
   * passed to readdir, closedir and fsyncdir.
   *
   * Introduced in version 2.3
   */
  int (*opendir) (const char *,
                  fuse_file_info_t *);

  /** Read directory
   *
   * This supersedes the old getdir() interface.  New applications
   * should use this.
   *
   * The filesystem may choose between two modes of operation:
   *
   * 1) The readdir implementation ignores the offset parameter, and
   * passes zero to the filler function's offset.  The filler
   * function will not return '1' (unless an error happens), so the
   * whole directory is read in a single readdir operation.  This
   * works just like the old getdir() method.
   *
   * 2) The readdir implementation keeps track of the offsets of the
   * directory entries.  It uses the offset parameter and always
   * passes non-zero offset to the filler function.  When the buffer
   * is full (or an error happens) the filler function will return
   * '1'.
   *
   * Introduced in version 2.3
   */
  int (*readdir)(const fuse_file_info_t *,
                 fuse_dirents_t *);

  int (*readdir_plus)(const fuse_file_info_t *,
                      fuse_dirents_t *);


  /** Release directory
   *
   * Introduced in version 2.3
   */
  int (*releasedir) (const fuse_file_info_t *);

  /** Synchronize directory contents
   *
   * If the datasync parameter is non-zero, then only the user data
   * should be flushed, not the meta data
   *
   * Introduced in version 2.3
   */
  int (*fsyncdir) (const fuse_file_info_t *, int);

  /**
   * Initialize filesystem
   *
   * The return value will passed in the private_data field of
   * fuse_context to all file operations and as a parameter to the
   * destroy() method.
   *
   * Introduced in version 2.3
   * Changed in version 2.6
   */
  void *(*init) (struct fuse_conn_info *conn);

  /**
   * Clean up filesystem
   *
   * Called on filesystem exit.
   *
   * Introduced in version 2.3
   */
  void (*destroy) (void);

  /**
   * Check file access permissions
   *
   * This will be called for the access() system call.  If the
   * 'default_permissions' mount option is given, this method is not
   * called.
   *
   * This method is not called under Linux kernel versions 2.4.x
   *
   * Introduced in version 2.5
   */
  int (*access) (const char *, int);

  /**
   * Create and open a file
   *
   * If the file does not exist, first create it with the specified
   * mode, and then open it.
   *
   * If this method is not implemented or under Linux kernel
   * versions earlier than 2.6.15, the mknod() and open() methods
   * will be called instead.
   *
   * Introduced in version 2.5
   */
  int (*create) (const char *, mode_t, fuse_file_info_t *);

  /**
   * Change the size of an open file
   *
   * This method is called instead of the truncate() method if the
   * truncation was invoked from an ftruncate() system call.
   *
   * If this method is not implemented or under Linux kernel
   * versions earlier than 2.6.15, the truncate() method will be
   * called instead.
   *
   * Introduced in version 2.5
   */
  int (*ftruncate) (const uint64_t, off_t);

  /**
   * Get attributes from an open file
   *
   * This method is called instead of the getattr() method if the
   * file information is available.
   *
   * Currently this is only called after the create() method if that
   * is implemented (see above).  Later it may be called for
   * invocations of fstat() too.
   *
   * Introduced in version 2.5
   */
  int (*fgetattr) (const uint64_t, struct stat *, fuse_timeouts_t *);

  /**
   * Perform POSIX file locking operation
   *
   * The cmd argument will be either F_GETLK, F_SETLK or F_SETLKW.
   *
   * For the meaning of fields in 'struct flock' see the man page
   * for fcntl(2).  The l_whence field will always be set to
   * SEEK_SET.
   *
   * For checking lock ownership, the 'fuse_file_info->owner'
   * argument must be used.
   *
   * For F_GETLK operation, the library will first check currently
   * held locks, and if a conflicting lock is found it will return
   * information without calling this method.	 This ensures, that
   * for local locks the l_pid field is correctly filled in.	The
   * results may not be accurate in case of race conditions and in
   * the presence of hard links, but it's unlikely that an
   * application would rely on accurate GETLK results in these
   * cases.  If a conflicting lock is not found, this method will be
   * called, and the filesystem may fill out l_pid by a meaningful
   * value, or it may leave this field zero.
   *
   * For F_SETLK and F_SETLKW the l_pid field will be set to the pid
   * of the process performing the locking operation.
   *
   * Note: if this method is not implemented, the kernel will still
   * allow file locking to work locally.  Hence it is only
   * interesting for network filesystems and similar.
   *
   * Introduced in version 2.6
   */
  int (*lock) (const fuse_file_info_t *,
               int cmd,
               struct flock *);

  /**
   * Change the access and modification times of a file with
   * nanosecond resolution
   *
   * This supersedes the old utime() interface.  New applications
   * should use this.
   *
   * See the utimensat(2) man page for details.
   *
   * Introduced in version 2.6
   */
  int (*utimens)(const char *, const struct timespec tv[2]);
  int (*futimens)(const uint64_t fh, const struct timespec tv[2]);

  /**
   * Map block index within file to block index within device
   *
   * Note: This makes sense only for block device backed filesystems
   * mounted with the 'blkdev' option
   *
   * Introduced in version 2.6
   */
  int (*bmap) (const char *, size_t blocksize, uint64_t *idx);

  /**
   * Ioctl
   *
   * flags will have FUSE_IOCTL_COMPAT set for 32bit ioctls in
   * 64bit environment.  The size and direction of data is
   * determined by _IOC_*() decoding of cmd.  For _IOC_NONE,
   * data will be NULL, for _IOC_WRITE data is out area, for
   * _IOC_READ in area and if both are set in/out area.  In all
   * non-NULL cases, the area is of _IOC_SIZE(cmd) bytes.
   *
   * If flags has FUSE_IOCTL_DIR then the fuse_file_info refers to a
   * directory file handle.
   *
   * Introduced in version 2.8
   */
  int (*ioctl) (const fuse_file_info_t *ffi,
                unsigned long           cmd,
                void                   *arg,
                unsigned int            flags,
                void                   *data,
                uint32_t               *out_bufsz);

  /**
   * Poll for IO readiness events
   *
   * Note: If ph is non-NULL, the client should notify
   * when IO readiness events occur by calling
   * fuse_notify_poll() with the specified ph.
   *
   * Regardless of the number of times poll with a non-NULL ph
   * is received, single notification is enough to clear all.
   * Notifying more times incurs overhead but doesn't harm
   * correctness.
   *
   * The callee is responsible for destroying ph with
   * fuse_pollhandle_destroy() when no longer in use.
   *
   * Introduced in version 2.8
   */
  int (*poll) (const fuse_file_info_t *ffi,
               fuse_pollhandle_t      *ph,
               unsigned               *reventsp);

  /** Write contents of buffer to an open file
   *
   * Similar to the write() method, but data is supplied in a
   * generic buffer.  Use fuse_buf_copy() to transfer data to
   * the destination.
   *
   * Introduced in version 2.9
   */
  int (*write) (const fuse_file_info_t *ffi,
                const char             *data,
                size_t                  size,
                off_t                   off);

  /** Store data from an open file in a buffer
   *
   * Similar to the read() method, but data is stored and
   * returned in a generic buffer.
   *
   * No actual copying of data has to take place, the source
   * file descriptor may simply be stored in the buffer for
   * later data transfer.
   *
   * The buffer must be allocated dynamically and stored at the
   * location pointed to by bufp.  If the buffer contains memory
   * regions, they too must be allocated using malloc().  The
   * allocated memory will be freed by the caller.
   *
   * Introduced in version 2.9
   */
  int (*read)(const fuse_file_info_t *ffi,
              char                   *buf,
              size_t                  size,
              off_t                   off);
  /**
   * Perform BSD file locking operation
   *
   * The op argument will be either LOCK_SH, LOCK_EX or LOCK_UN
   *
   * Nonblocking requests will be indicated by ORing LOCK_NB to
   * the above operations
   *
   * For more information see the flock(2) manual page.
   *
   * Additionally fi->owner will be set to a value unique to
   * this open file.  This same value will be supplied to
   * ->release() when the file is released.
   *
   * Note: if this method is not implemented, the kernel will still
   * allow file locking to work locally.  Hence it is only
   * interesting for network filesystems and similar.
   *
   * Introduced in version 2.9
   */
  int (*flock) (const fuse_file_info_t *, int op);

  /**
   * Allocates space for an open file
   *
   * This function ensures that required space is allocated for specified
   * file.  If this function returns success then any subsequent write
   * request to specified range is guaranteed not to fail because of lack
   * of space on the file system media.
   *
   * Introduced in version 2.9.1
   */
  int (*fallocate) (const uint64_t, int, off_t, off_t);

  /**
   * Copy a range of data from one file to another
   *
   * Performs an optimized copy between two file descriptors without
   * the additional cost of transferring data through the FUSE kernel
   * module to user space (glibc) and then back into the FUSE filesystem
   * again.
   *
   * In case this method is not implemented, glibc falls back to
   * reading data from the source and writing to the
   * destination. Effectively doing an inefficient copy of the
   * data.
   */
  ssize_t (*copy_file_range)(const fuse_file_info_t *fi_in,
                             off_t                   offset_in,
                             const fuse_file_info_t *fi_out,
                             off_t                   offset_out,
                             size_t                  size,
                             int                     flags);

  ssize_t (*setupmapping)(uint64_t *fh_,
                          uint64_t  foffset_,
                          uint64_t  len_,
                          uint64_t  flags_,
                          uint64_t  moffset_);

  int (*removemapping)();
  int (*syncfs)();
  int (*tmpfile)(const char *, mode_t, fuse_file_info_t *);
  int (*statx)(const char        *fusepath,
               const uint32_t     flags,
               const uint32_t     mask,
               struct fuse_statx *st,
               fuse_timeouts_t   *timeout);
  int (*statx_fh)(const uint64_t     fh,
                  const uint32_t     flags,
                  const uint32_t     mask,
                  struct fuse_statx *st,
                  fuse_timeouts_t   *timeout);
};

/** Extra context that may be needed by some filesystems
 *
 * The uid, gid and pid fields are not filled in case of a writepage
 * operation.
 */
struct fuse_context
{
  uint64_t unique;
  uint64_t nodeid;
  uint32_t opcode;
  uid_t    uid;
  gid_t    gid;
  pid_t    pid;
  mode_t   umask;
  struct fuse *fuse;
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
/*
  int fuse_main(int argc, char *argv[], const struct fuse_operations *op);
*/
#define fuse_main(argc, argv, op)                    \
  fuse_main_real(argc, argv, op, sizeof(*(op)))

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
struct fuse *fuse_new(struct fuse_chan *ch, struct fuse_args *args,
                      const struct fuse_operations *op, size_t op_size);

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
void fuse_destroy(struct fuse *f);

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
int fuse_main_real(int argc, char *argv[], const struct fuse_operations *op, size_t op_size);

int  fuse_start_maintenance_thread(struct fuse *fuse);
void fuse_stop_maintenance_thread(struct fuse *fuse);

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

/**
 * Fuse filesystem object
 *
 * This is opaque object represents a filesystem layer
 */
struct fuse_fs;

/*
 * These functions call the relevant filesystem operation, and return
 * the result.
 *
 * If the operation is not defined, they return -ENOSYS, with the
 * exception of fuse_fs_open, fuse_fs_release, fuse_fs_opendir,
 * fuse_fs_releasedir and fuse_fs_statfs, which return 0.
 */

int fuse_notify_poll(fuse_pollhandle_t *ph);

/**
 * Create a new fuse filesystem object
 *
 * This is usually called from the factory of a fuse module to create
 * a new instance of a filesystem.
 *
 * @param op the filesystem operations
 * @param op_size the size of the fuse_operations structure
 * @return a new filesystem object
 */
struct fuse_fs *fuse_fs_new(const struct fuse_operations *op, size_t op_size);

/* ----------------------------------------------------------- *
 * Advanced API for event handling, don't worry about this...  *
 * ----------------------------------------------------------- */

/* NOTE: the following functions are deprecated, and will be removed
   from the 3.0 API.  Use the lowlevel session functions instead */

/** Function type used to process commands */
typedef void (*fuse_processor_t)(struct fuse *, struct fuse_cmd *, void *);

/** This is the part of fuse_main() before the event loop */
struct fuse *fuse_setup(int argc, char *argv[],
                        const struct fuse_operations *op, size_t op_size,
                        char **mountpoint);

/** This is the part of fuse_main() after the event loop */
void fuse_teardown(struct fuse *fuse, char *mountpoint);

/** Multi threaded event loop, which calls the custom command
    processor function */
int fuse_loop_mt_proc(struct fuse *f, fuse_processor_t proc, void *data);

/** Return the exited flag, which indicates if fuse_exit() has been
    called */
int fuse_exited(struct fuse *f);

/** This function is obsolete and implemented as a no-op */
void fuse_set_getcontext_func(struct fuse_context *(*func)(void));

/** Get session from fuse object */
struct fuse_session *fuse_get_session(struct fuse *f);

void fuse_gc1();
void fuse_gc();
void fuse_invalidate_all_nodes();

int fuse_get_dev_fuse_fd(const struct fuse_context *fc);
int fuse_passthrough_open(const struct fuse_context *fc,
                          const int                  fd);
int fuse_passthrough_close(const struct fuse_context *fc,
                           const int                  backing_id);

EXTERN_C_END

#endif /* _FUSE_H_ */
