# Runtime Interfaces

## Runtime Config

### .mergerfs pseudo file

```
<mountpoint>/.mergerfs
```

There is a pseudo file available at the mount point which allows for
the runtime modification of certain **mergerfs** options. The file
will not show up in **readdir** but can be **stat**'ed and manipulated
via [{list,get,set}xattrs](http://linux.die.net/man/2/listxattr)
calls.

Any changes made at runtime are **not** persisted. If you wish for
values to persist they must be included as options wherever you
configure the mounting of mergerfs (/etc/fstab).


#### Keys

Use `getfattr -d /mountpoint/.mergerfs` or `xattr -l
/mountpoint/.mergerfs` to see all supported keys. Some are
informational and therefore read-only. `setxattr` will return EINVAL
(invalid argument) on read-only keys.


#### Values

Same as the command line.


##### user.mergerfs.branches

Used to query or modify the list of branches. When modifying there are
several shortcuts to easy manipulation of the list.

| Value    | Description                |
| -------- | -------------------------- |
| [list]   | set                        |
| +<[list] | prepend                    |
| +>[list] | append                     |
| -[list]  | remove all values provided |
| -<       | remove first in list       |
| ->       | remove last in list        |

`xattr -w user.mergerfs.branches +</mnt/drive3 /mnt/pool/.mergerfs`

The `=NC`, `=RO`, `=RW` syntax works just as on the command line.

##### Example

```
[trapexit:/mnt/mergerfs] $ getfattr -d .mergerfs
user.mergerfs.branches="/mnt/a=RW:/mnt/b=RW"
user.mergerfs.minfreespace="4294967295"
user.mergerfs.moveonenospc="false"
...

[trapexit:/mnt/mergerfs] $ getfattr -n user.mergerfs.category.search .mergerfs
user.mergerfs.category.search="ff"

[trapexit:/mnt/mergerfs] $ setfattr -n user.mergerfs.category.search -v newest .mergerfs
[trapexit:/mnt/mergerfs] $ getfattr -n user.mergerfs.category.search .mergerfs
user.mergerfs.category.search="newest"
```

### file / directory xattrs

While they won't show up when using `getfattr` **mergerfs** offers a
number of special xattrs to query information about the files
served. To access the values you will need to issue a
[getxattr](http://linux.die.net/man/2/getxattr) for one of the
following:

- **user.mergerfs.basepath**: the base mount point for the file given the current getattr policy
- **user.mergerfs.relpath**: the relative path of the file from the perspective of the mount point
- **user.mergerfs.fullpath**: the full path of the original file given the getattr policy
- **user.mergerfs.allpaths**: a NUL ('\0') separated list of full paths to all files found

## Signals

- USR1: This will cause mergerfs to send invalidation notifications to
  the kernel for all files. This will cause all unused files to be
  released from memory.
- USR2: Trigger a general cleanup of currently unused memory. A more
  thorough version of what happens every ~15 minutes.

## ioctl

Found in `fuse_ioctl.cpp`:

```C++
typedef char IOCTL_BUF[4096];
#define IOCTL_APP_TYPE             0xDF
#define IOCTL_FILE_INFO            _IOWR(IOCTL_APP_TYPE,0,IOCTL_BUF)
#define IOCTL_GC                   _IO(IOCTL_APP_TYPE,1)
#define IOCTL_GC1                  _IO(IOCTL_APP_TYPE,2)
#define IOCTL_INVALIDATE_ALL_NODES _IO(IOCTL_APP_TYPE,3)
```

- IOCTL_FILE_INFO: Same as the "file / directory xattrs" mentioned
  above. Use a buffer size of 4096 bytes. Pass in a string of
  "basepath", "relpath", "fullpath", or "allpaths". Receive details in
  same buffer.
- IOCTL_GC: Triggers a thorough garbage collection of excess
  memory. Same as SIGUSR2.
- IOCTL_GC1: Triggers a simple garbage collection of excess
  memory. Same as what happens every 15 minutes normally.
- IOCTL_INVALIDATE_ALL_NODES: Same as SIGUSR1. Send invalidation
  notifications to the kernel for all files causing unused files to be
  released from memory.
