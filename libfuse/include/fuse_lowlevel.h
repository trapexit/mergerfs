#pragma once

#include "extern_c.h"
#include "fuse_common.h"
#include "fuse_kernel.h"
#include "fuse_req.h"

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <utime.h>

EXTERN_C_BEGIN

/* ----------------------------------------------------------- *
 * Miscellaneous definitions				       *
 * ----------------------------------------------------------- */

/** The node ID of the root inode */
#define FUSE_ROOT_ID 1

/**
 * Session
 *
 * This provides hooks for processing requests, and exiting
 */
struct fuse_session;

/**
 * Channel
 *
 * A communication channel, providing hooks for sending and receiving
 * messages
 */
struct fuse_chan;

/** Directory entry parameters supplied to fuse_reply_entry() */
struct fuse_entry_param
{
  /** Unique inode number
   *
   * In lookup, zero means negative entry (from version 2.5)
   * Returning ENOENT also means negative entry, but by setting zero
   * ino the kernel may cache negative entries for entry_timeout
   * seconds.
   */
  uint64_t ino;

  /** Generation number for this entry.
   *
   * If the file system will be exported over NFS, the
   * ino/generation pairs need to be unique over the file
   * system's lifetime (rather than just the mount time). So if
   * the file system reuses an inode after it has been deleted,
   * it must assign a new, previously unused generation number
   * to the inode at the same time.
   *
   * The generation must be non-zero, otherwise FUSE will treat
   * it as an error.
   *
   */
  uint64_t generation;


  /** Inode attributes.
   *
   * Even if attr_timeout == 0, attr must be correct. For example,
   * for open(), FUSE uses attr.st_size from lookup() to determine
   * how many bytes to request. If this value is not correct,
   * incorrect data will be returned.
   */
  struct stat attr;

  fuse_timeouts_t timeout;
};

/* ----------------------------------------------------------- *
 * Request methods and replies				       *
 * ----------------------------------------------------------- */

/**
 * Low level filesystem operations
 *
 * Most of the methods (with the exception of init and destroy)
 * receive a request handle (fuse_req_t) as their first argument.
 * This handle must be passed to one of the specified reply functions.
 *
 * This may be done inside the method invocation, or after the call
 * has returned.  The request handle is valid until one of the reply
 * functions is called.
 *
 * Other pointer arguments (name, fuse_file_info, etc) are not valid
 * after the call has returned, so if they are needed later, their
 * contents have to be copied.
 *
 * The filesystem sometimes needs to handle a return value of -ENOENT
 * from the reply function, which means, that the request was
 * interrupted, and the reply discarded.  For example if
 * fuse_reply_open() return -ENOENT means, that the release method for
 * this file will not be called.
 */
struct fuse_lowlevel_ops
{
  void (*access)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*bmap)(fuse_req_t *req, const struct fuse_in_header *hdr);
  void (*copy_file_range)(fuse_req_t *req, const struct fuse_in_header *hdr);
  void (*create)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*destroy)(void *userdata);
  void (*fallocate)(fuse_req_t *req, const struct fuse_in_header *hdr);
  void (*flock)(fuse_req_t *req, uint64_t ino, fuse_file_info_t *fi, int op);
  void (*flush)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*forget)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*forget_multi)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*fsync)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*fsyncdir)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*getattr)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*getlk)(fuse_req_t *req, const struct fuse_in_header *hdr);
  void (*getxattr)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*init)(void *userdata, struct fuse_conn_info *conn);
  void (*ioctl)(fuse_req_t *req, const struct fuse_in_header *hdr);
  void (*link)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*listxattr)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*lookup)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*lseek)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*mkdir)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*mknod)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*open)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*opendir)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*poll)(fuse_req_t *req, const struct fuse_in_header *hdr);
  void (*read)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*readdir)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*readdir_plus)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*readlink)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*release)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*releasedir)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*removemapping)(fuse_req_t *req, const struct fuse_in_header *hdr);
  void (*removexattr)(fuse_req_t *req, const struct fuse_in_header *hdr);
  void (*rename)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*rename2)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*retrieve_reply)(fuse_req_t *req, void *cookie, uint64_t ino, off_t offset);
  void (*rmdir)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*setattr)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*setlk)(fuse_req_t *req, uint64_t ino, fuse_file_info_t *fi, struct flock *lock, int sleep);
  void (*setupmapping)(fuse_req_t *req, const struct fuse_in_header *hdr);
  void (*setxattr)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*statfs)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*statx)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*symlink)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*syncfs)(fuse_req_t *req, const struct fuse_in_header *hdr);
  void (*tmpfile)(fuse_req_t *req, const struct fuse_in_header *hdr);
  void (*unlink)(fuse_req_t *req, struct fuse_in_header *hdr);
  void (*write)(fuse_req_t *req, struct fuse_in_header *hdr);
};

/**
 * Reply with an error code or success
 *
 * Possible requests:
 *   all except forget
 *
 * unlink, rmdir, rename, flush, release, fsync, fsyncdir, setxattr,
 * removexattr and setlk may send a zero code
 *
 * @param req request handle
 * @param err the positive error value, or zero for success
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_err(fuse_req_t *req, int err);

/**
 * Don't send reply
 *
 * Possible requests:
 *   forget
 *
 * @param req request handle
 */
void fuse_reply_none(fuse_req_t *req);

/**
 * Reply with a directory entry
 *
 * Possible requests:
 *   lookup, mknod, mkdir, symlink, link
 *
 * Side effects:
 *   increments the lookup count on success
 *
 * @param req request handle
 * @param e the entry parameters
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_entry(fuse_req_t *req, const struct fuse_entry_param *e);

/**
 * Reply with a directory entry and open parameters
 *
 * currently the following members of 'fi' are used:
 *   fh, direct_io, keep_cache
 *
 * Possible requests:
 *   create
 *
 * Side effects:
 *   increments the lookup count on success
 *
 * @param req request handle
 * @param e the entry parameters
 * @param fi file information
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_create(fuse_req_t *req,
                      const struct fuse_entry_param *e,
                      const fuse_file_info_t *fi);

/**
 * Reply with attributes
 *
 * Possible requests:
 *   getattr, setattr
 *
 * @param req request handle
 * @param attr the attributes
 * @param attr_timeout	validity timeout (in seconds) for the attributes
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_attr(fuse_req_t        *req,
                    const struct stat *attr,
                    const uint64_t     timeout);


int fuse_reply_statx(fuse_req_t        *req,
                     int                flags,
                     struct fuse_statx *st,
                     const uint64_t     timeout);

/**
 * Reply with the contents of a symbolic link
 *
 * Possible requests:
 *   readlink
 *
 * @param req request handle
 * @param link symbolic link contents
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_readlink(fuse_req_t *req, const char *link);

/**
 * Reply with open parameters
 *
 * currently the following members of 'fi' are used:
 *   fh, direct_io, keep_cache
 *
 * Possible requests:
 *   open, opendir
 *
 * @param req request handle
 * @param fi file information
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_open(fuse_req_t *req,
                    const fuse_file_info_t *fi);

/**
 * Reply with number of bytes written
 *
 * Possible requests:
 *   write
 *
 * @param req request handle
 * @param count the number of bytes written
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_write(fuse_req_t *req, size_t count);

/**
 * Reply with data
 *
 * Possible requests:
 *   read, readdir, getxattr, listxattr
 *
 * @param req request handle
 * @param buf buffer containing data
 * @param size the size of data in bytes
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_buf(fuse_req_t *req, const char *buf, size_t size);

int fuse_reply_data(fuse_req_t *req,
                    char       *buf,
                    size_t      bufsize);

/**
 * Reply with data vector
 *
 * Possible requests:
 *   read, readdir, getxattr, listxattr
 *
 * @param req request handle
 * @param iov the vector containing the data
 * @param count the size of vector
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_iov(fuse_req_t *req, const struct iovec *iov, int count);

/**
 * Reply with filesystem statistics
 *
 * Possible requests:
 *   statfs
 *
 * @param req request handle
 * @param stbuf filesystem statistics
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_statfs(fuse_req_t *req, const struct statvfs *stbuf);

/**
 * Reply with needed buffer size
 *
 * Possible requests:
 *   getxattr, listxattr
 *
 * @param req request handle
 * @param count the buffer size needed in bytes
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_xattr(fuse_req_t *req, size_t count);

/**
 * Reply with file lock information
 *
 * Possible requests:
 *   getlk
 *
 * @param req request handle
 * @param lock the lock information
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_lock(fuse_req_t *req, const struct flock *lock);

/**
 * Reply with block index
 *
 * Possible requests:
 *   bmap
 *
 * @param req request handle
 * @param idx block index within device
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_bmap(fuse_req_t *req, uint64_t idx);

/**
 * Reply to ask for data fetch and output buffer preparation.  ioctl
 * will be retried with the specified input data fetched and output
 * buffer prepared.
 *
 * Possible requests:
 *   ioctl
 *
 * @param req request handle
 * @param in_iov iovec specifying data to fetch from the caller
 * @param in_count number of entries in in_iov
 * @param out_iov iovec specifying addresses to write output to
 * @param out_count number of entries in out_iov
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_ioctl_retry(fuse_req_t *req,
                           const struct iovec *in_iov, size_t in_count,
                           const struct iovec *out_iov, size_t out_count);

/**
 * Reply to finish ioctl
 *
 * Possible requests:
 *   ioctl
 *
 * @param req request handle
 * @param result result to be passed to the caller
 * @param buf buffer containing output data
 * @param size length of output data
 */
int fuse_reply_ioctl(fuse_req_t *req, int result, const void *buf, uint32_t size);

/**
 * Reply to finish ioctl with iov buffer
 *
 * Possible requests:
 *   ioctl
 *
 * @param req request handle
 * @param result result to be passed to the caller
 * @param iov the vector containing the data
 * @param count the size of vector
 */
int fuse_reply_ioctl_iov(fuse_req_t *req, int result, const struct iovec *iov,
                         int count);

/**
 * Reply with poll result event mask
 *
 * @param req request handle
 * @param revents poll result event mask
 */
int fuse_reply_poll(fuse_req_t *req, unsigned revents);

/* ----------------------------------------------------------- *
 * Notification						       *
 * ----------------------------------------------------------- */

/**
 * Notify IO readiness event
 *
 * For more information, please read comment for poll operation.
 *
 * @param ph poll handle to notify IO readiness event for
 */
int fuse_lowlevel_notify_poll(fuse_pollhandle_t *ph);

/**
 * Notify to invalidate cache for an inode
 *
 * @param ch the channel through which to send the invalidation
 * @param ino the inode number
 * @param off the offset in the inode where to start invalidating
 *            or negative to invalidate attributes only
 * @param len the amount of cache to invalidate or 0 for all
 * @return zero for success, -errno for failure
 */
int fuse_lowlevel_notify_inval_inode(struct fuse_chan *ch, uint64_t ino,
                                     off_t off, off_t len);

/**
 * Notify to invalidate parent attributes and the dentry matching
 * parent/name
 *
 * To avoid a deadlock don't call this function from a filesystem operation and
 * don't call it with a lock held that can also be held by a filesystem
 * operation.
 *
 * @param ch the channel through which to send the invalidation
 * @param parent inode number
 * @param name file name
 * @param namelen strlen() of file name
 * @return zero for success, -errno for failure
 */
int fuse_lowlevel_notify_inval_entry(struct fuse_chan *ch, uint64_t parent,
                                     const char *name, size_t namelen);

/**
 * Notify to invalidate parent attributes and delete the dentry matching
 * parent/name if the dentry's inode number matches child (otherwise it
 * will invalidate the matching dentry).
 *
 * To avoid a deadlock don't call this function from a filesystem operation and
 * don't call it with a lock held that can also be held by a filesystem
 * operation.
 *
 * @param ch the channel through which to send the notification
 * @param parent inode number
 * @param child inode number
 * @param name file name
 * @param namelen strlen() of file name
 * @return zero for success, -errno for failure
 */
int fuse_lowlevel_notify_delete(struct fuse_chan *ch,
                                uint64_t parent, uint64_t child,
                                const char *name, size_t namelen);

/**
 * Store data to the kernel buffers
 *
 * Synchronously store data in the kernel buffers belonging to the
 * given inode.  The stored data is marked up-to-date (no read will be
 * performed against it, unless it's invalidated or evicted from the
 * cache).
 *
 * If the stored data overflows the current file size, then the size
 * is extended, similarly to a write(2) on the filesystem.
 *
 * If this function returns an error, then the store wasn't fully
 * completed, but it may have been partially completed.
 *
 * @param ch the channel through which to send the invalidation
 * @param ino the inode number
 * @param offset the starting offset into the file to store to
 * @param bufv buffer vector
 * @param flags flags controlling the copy
 * @return zero for success, -errno for failure
 */
int fuse_lowlevel_notify_store(struct fuse_chan *ch, uint64_t ino,
                               off_t offset, struct fuse_bufvec *bufv,
                               enum fuse_buf_copy_flags flags);
/**
 * Retrieve data from the kernel buffers
 *
 * Retrieve data in the kernel buffers belonging to the given inode.
 * If successful then the retrieve_reply() method will be called with
 * the returned data.
 *
 * Only present pages are returned in the retrieve reply.  Retrieving
 * stops when it finds a non-present page and only data prior to that is
 * returned.
 *
 * If this function returns an error, then the retrieve will not be
 * completed and no reply will be sent.
 *
 * This function doesn't change the dirty state of pages in the kernel
 * buffer.  For dirty pages the write() method will be called
 * regardless of having been retrieved previously.
 *
 * @param ch the channel through which to send the invalidation
 * @param ino the inode number
 * @param size the number of bytes to retrieve
 * @param offset the starting offset into the file to retrieve from
 * @param cookie user data to supply to the reply callback
 * @return zero for success, -errno for failure
 */
int fuse_lowlevel_notify_retrieve(struct fuse_chan *ch, uint64_t ino,
                                  size_t size, off_t offset, void *cookie);


/* ----------------------------------------------------------- *
 * Utility functions					       *
 * ----------------------------------------------------------- */

/**
 * Get the current supplementary group IDs for the specified request
 *
 * Similar to the getgroups(2) system call, except the return value is
 * always the total number of group IDs, even if it is larger than the
 * specified size.
 *
 * The current fuse kernel module in linux (as of 2.6.30) doesn't pass
 * the group list to userspace, hence this function needs to parse
 * "/proc/$TID/task/$TID/status" to get the group IDs.
 *
 * This feature may not be supported on all operating systems.  In
 * such a case this function will return -ENOSYS.
 *
 * @param req request handle
 * @param size size of given array
 * @param list array of group IDs to be filled in
 * @return the total number of supplementary group IDs or -errno on failure
 */
int fuse_req_getgroups(fuse_req_t *req, int size, gid_t list[]);

/* ----------------------------------------------------------- *
 * Filesystem setup					       *
 * ----------------------------------------------------------- */

/* Deprecated, don't use */
int fuse_lowlevel_is_lib_option(const char *opt);

/**
 * Create a low level session
 *
 * @param args argument vector
 * @param op the low level filesystem operations
 * @param op_size sizeof(struct fuse_lowlevel_ops)
 * @param userdata user data
 * @return the created session object, or NULL on failure
 */
struct fuse_session *fuse_lowlevel_new(struct fuse_args *args,
                                       const struct fuse_lowlevel_ops *op,
                                       size_t op_size, void *userdata);

/* ----------------------------------------------------------- *
 * Session interface					       *
 * ----------------------------------------------------------- */

struct fuse_session *fuse_session_new(void *data,
                                      void *receive_buf,
                                      void *process_buf,
                                      void *destroy);
void fuse_session_add_chan(struct fuse_session *se, struct fuse_chan *ch);
void fuse_session_remove_chan(struct fuse_chan *ch);
void fuse_session_destroy(struct fuse_session *se);
void fuse_session_exit(struct fuse_session *se);
int fuse_session_exited(struct fuse_session *se);
void fuse_session_reset(struct fuse_session *se);
void *fuse_session_data(struct fuse_session *se);
int fuse_session_receive(struct fuse_session *se,
                         struct fuse_buf *buf);
void fuse_session_process(struct fuse_session *se,
                          const void          *buf,
                          const size_t         bufsize);

int fuse_session_loop_mt(struct fuse_session *se,
                         const int            read_thread_count,
                         const int            process_thread_count,
                         const int            process_thread_queue_depth,
                         const char          *pin_threads_type);

/* ----------------------------------------------------------- *
 * Channel interface					       *
 * ----------------------------------------------------------- */

struct fuse_chan *fuse_chan_new(int fd, size_t bufsize);

/**
 * Query the file descriptor of the channel
 *
 * @param ch the channel
 * @return the file descriptor passed to fuse_chan_new()
 */
int fuse_chan_fd(struct fuse_chan *ch);

/**
 * Query the minimal receive buffer size
 *
 * @param ch the channel
 * @return the buffer size passed to fuse_chan_new()
 */
size_t fuse_chan_bufsize(struct fuse_chan *ch);

/**
 * Query the user data
 *
 * @param ch the channel
 * @return the user data passed to fuse_chan_new()
 */
void *fuse_chan_data(struct fuse_chan *ch);

/**
 * Query the session to which this channel is assigned
 *
 * @param ch the channel
 * @return the session, or NULL if the channel is not assigned
 */
struct fuse_session *fuse_chan_session(struct fuse_chan *ch);

void fuse_chan_destroy(struct fuse_chan *ch);

EXTERN_C_END
