# Runtime Interfaces

`mergerfs` has runtime interfaces allowing users to query certain
information, set config, and trigger certain activities while it is
running.

These interfaces come in the form of the [extended attributes
filesystem
interface](https://en.wikipedia.org/wiki/Extended_file_attributes),
[ioctl](https://en.wikipedia.org/wiki/Ioctl), and
[signals](https://en.wikipedia.org/wiki/Signal_(IPC)).


## xattr

This takes advantage of the POSIX extended attributes filesystem
API. Effectively namespaced `key=value` pairs which are associated
with a file. Since `mergerfs` uses primarily `key=value` pairs for
config it fits well.


### .mergerfs pseudo file

```
<mountpoint>/.mergerfs
```

`mergerfs` provides this pseudo file allows for the runtime
modification of certain options. The file will not show up in
`readdir` but can be `stat`'ed and manipulated via
[{list,get,set}xattrs](http://linux.die.net/man/2/listxattr) calls.

Any changes made at runtime are **NOT** persisted. If you wish for
values to persist they must be included as options wherever you
configure the mounting of mergerfs (/etc/fstab, systemd, etc.).

As of v2.41.0 the mount point can also be used but the results of the
underlying directories will be mixed in with the `mergerfs` values.


#### Keys

Use `getfattr -d /mountpoint/.mergerfs` or `xattr -l
/mountpoint/.mergerfs` to see all supported keys. It is effectively
the same as the [options](config/options.md) prefixed with
`user.mergerfs.`. Some are informational and therefore read-only such
as certain options which set FUSE options that can not be modified
after setup.

Example: option `cache.files` would be `user.mergerfs.cache.files`.


#### Values

Same as the [command line options](config/options.md).


#### Getting

`getfattr -n user.mergerfs.branches /mountpoint/.mergerfs`

or

`getfattr -n user.mergerfs.branches /mountpoint`

`ENOATTR` will be returned if the key doesn't exist as normal with
`getxattr`.


#### Setting

`setfattr -n user.mergerfs.branches -v VALUE /mountpoint/.mergerfs`

or

`setfattr -n user.mergerfs.branches -v VALUE /mountpoint`

`setxattr` will return `EROFS` (Read-only filesystem) on read-only
keys. `ENOATTR` will be returned if the key doesn't exist. If the
value attempting to be set is not valid `EINVAL` will be returned.


##### user.mergerfs.branches

`branches` has the ability to understand some simple instructions to
make manipulation of the list easier. The `[list]` is simply what is
described in the [branches](config/branches.md) docs.

| Value    | Description                |
| -------- | -------------------------- |
| [list]   | set                        |
| +<[list] | prepend to existing list   |
| +>[list] | append to existing list    |
| -[list]  | remove all values provided |
| -<       | remove first in list       |
| ->       | remove last in list        |

**NOTE:** if the value of `branches` is set to something invalid /
non-existant `mergerfs` will return a bogus entry when the mount point
directory is `stat`'ed and create a fake file entry when listing the
directory telling the user "error: no valid mergerfs branch found,
check config".


##### Example

```
[trapexit:/mnt/mergerfs] $ getfattr -d .mergerfs
user.mergerfs.branches="/mnt/a=RW:/mnt/b=RW"
user.mergerfs.minfreespace="4294967295"
user.mergerfs.moveonenospc="false"
...

[trapexit:/mnt/mergerfs] $ getfattr -n user.mergerfs.category.create .mergerfs
user.mergerfs.category.search="mfs"

[trapexit:/mnt/mergerfs] $ setfattr -n user.mergerfs.category.create -v pfrd .mergerfs
[trapexit:/mnt/mergerfs] $ getfattr -n user.mergerfs.category.create .mergerfs
user.mergerfs.category.search="prfd"

[trapexit:/mnt/mergerfs] $ setfattr -n user.mergerfs.branches -v "'+</mnt/c=RO .mergerfs
[trapexit:/mnt/mergerfs] $ getfattr -n user.mergerfs.branches .mergerfs
user.mergerfs.branches="/mnt/c=RO:/mnt/a=RW:/mnt/b=RW"
```

### file / directory xattrs

While they won't show up when using `getfattr` **mergerfs** offers a
number of special xattrs to query information about the files
served. To access the values you will need to issue a
[getxattr](http://linux.die.net/man/2/getxattr) for one of the
following:

| Key | Return Value |
| --- | ------------ |
| user.mergerfs.basepath | The base mount point for the file given the current `getattr` policy. |
| user.mergerfs.relpath | The relative path of the file from the perspective of the mount point. |
| user.mergerfs.fullpath | The full path of the original file given the `getattr` policy. |
| user.mergerfs.allpaths | A NULL ('\0') separated list of full paths to all files found. |


## ioctl

The primary reason to use `ioctl` rather than `xattr` is if `xattr` is
disabled. It used to be that FUSE could issue certain xattr requests
while writing data which could significantly slow down IO but modern
kernels have a feature to limit that behavior making disabling `xattr`
less common.

Found in [mergerfs_ioctl.hpp](https://github.com/trapexit/mergerfs/blob/master/src/mergerfs_ioctl.hpp):

```C++
#define IOCTL_BUF_SIZE ((1 << _IOC_SIZEBITS) - 1)
typedef char IOCTL_BUF[IOCTL_BUF_SIZE];
#define IOCTL_APP_TYPE             0xDF
#define IOCTL_FILE_INFO            _IOWR(IOCTL_APP_TYPE,0,IOCTL_BUF)
#define IOCTL_GC                   _IO(IOCTL_APP_TYPE,1)
#define IOCTL_GC1                  _IO(IOCTL_APP_TYPE,2)
#define IOCTL_INVALIDATE_ALL_NODES _IO(IOCTL_APP_TYPE,3)
#define IOCTL_INVALIDATE_GID_CACHE _IO(IOCTL_APP_TYPE,4)
#define IOCTL_CLEAR_GID_CACHE      _IO(IOCTL_APP_TYPE,5)
```

| ioctl op code | description |
| ------------- | ----------- |
| IOCTL_FILE_INFO | Same as the "file / directory xattrs" mentioned above. Use a buffer size of bytes. Pass in a string of "basepath", "relpath", "fullpath", or "allpaths". Receive details in same buffer. |
| IOCTL_GC | Triggers a thorough garbage collection of excess memory. Same as SIGUSR2. |
| IOCTL_GC1 | Triggers a simple garbage collection of excess memory. Same as what happens every 15 minutes normally. |
| IOCTL_INVALIDATE_ALL_NODES | Same as SIGUSR1. Send invalidation notifications to the kernel for all files causing unused files to be released from memory. |
| IOCTL_INVALIDATE_GID_CACHE | Invalidates all gid cache entries. |
| IOCTL_CLEAR_GID_CACHE | Clears gid cache entries. |

The max buffer size of an `ioctl` call is 16KiB - 1 which is less than
the typical max of 64KiB for xattr values but should be sufficent for
most situations.


## Signals

- USR1: This will cause mergerfs to send invalidation notifications to
  the kernel for all files. This will cause all unused files to be
  released from memory.
- USR2: Trigger a general cleanup of currently unused memory. A more
  thorough version of what happens every ~15 minutes. Also fully
  clears the gid cache.

