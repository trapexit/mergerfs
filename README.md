% mergerfs(1) mergerfs user manual
% Antonio SJ Musumeci <trapexit@spawn.link>
% 2019-06-03

# NAME

mergerfs - a featureful union filesystem


# SYNOPSIS

mergerfs -o&lt;options&gt; &lt;branches&gt; &lt;mountpoint&gt;


# DESCRIPTION

**mergerfs** is a union filesystem geared towards simplifying storage and management of files across numerous commodity storage devices. It is similar to **mhddfs**, **unionfs**, and **aufs**.


# FEATURES

* Runs in userspace (FUSE)
* Configurable behaviors / file placement
* Support for extended attributes (xattrs)
* Support for file attributes (chattr)
* Runtime configurable (via xattrs)
* Safe to run as root
* Opportunistic credential caching
* Works with heterogeneous filesystem types
* Handling of writes to full drives (transparently move file to drive with capacity)
* Handles pool of read-only and read/write drives
* Can turn read-only files into symlinks to underlying file
* Hard link copy-on-write / CoW
* supports POSIX ACLs


# How it works

mergerfs logically merges multiple paths together. Think a union of sets. The file/s or directory/s acted on or presented through mergerfs are based on the policy chosen for that particular action. Read more about policies below.

```
A         +      B        =       C
/disk1           /disk2           /merged
|                |                |
+-- /dir1        +-- /dir1        +-- /dir1
|   |            |   |            |   |
|   +-- file1    |   +-- file2    |   +-- file1
|                |   +-- file3    |   +-- file2
+-- /dir2        |                |   +-- file3
|   |            +-- /dir3        |
|   +-- file4        |            +-- /dir2
|                     +-- file5   |   |
+-- file6                         |   +-- file4
                                  |
                                  +-- /dir3
                                  |   |
                                  |   +-- file5
                                  |
                                  +-- file6
```

mergerfs does **not** support the copy-on-write (CoW) behavior found in **aufs** and **overlayfs**. You can **not** mount a read-only filesystem and write to it. However, mergerfs will ignore read-only drives when creating new files so you can mix read-write and read-only drives.


# OPTIONS

### mount options

* **allow_other**: A libfuse option which allows users besides the one which ran mergerfs to see the filesystem. This is required for most use-cases.
* **minfreespace=SIZE**: The minimum space value used for creation policies. Understands 'K', 'M', and 'G' to represent kilobyte, megabyte, and gigabyte respectively. (default: 4G)
* **moveonenospc=BOOL**: When enabled if a **write** fails with **ENOSPC** or **EDQUOT** a scan of all drives will be done looking for the drive with the most free space which is at least the size of the file plus the amount which failed to write. An attempt to move the file to that drive will occur (keeping all metadata possible) and if successful the original is unlinked and the write retried. (default: false)
* **use_ino**: Causes mergerfs to supply file/directory inodes rather than libfuse. While not a default it is recommended it be enabled so that linked files share the same inode value.
* **dropcacheonclose=BOOL**: When a file is requested to be closed call `posix_fadvise` on it first to instruct the kernel that we no longer need the data and it can drop its cache. Recommended when **cache.files=partial|full|auto-full** to limit double caching. (default: false)
* **symlinkify=BOOL**: When enabled and a file is not writable and its mtime or ctime is older than **symlinkify_timeout** files will be reported as symlinks to the original files. Please read more below before using. (default: false)
* **symlinkify_timeout=INT**: Time to wait, in seconds, to activate the **symlinkify** behavior. (default: 3600)
* **nullrw=BOOL**: Turns reads and writes into no-ops. The request will succeed but do nothing. Useful for benchmarking mergerfs. (default: false)
* **ignorepponrename=BOOL**: Ignore path preserving on rename. Typically rename and link act differently depending on the policy of `create` (read below). Enabling this will cause rename and link to always use the non-path preserving behavior. This means files, when renamed or linked, will stay on the same drive. (default: false)
* **security_capability=BOOL**: If false return ENOATTR when xattr security.capability is queried. (default: true)
* **xattr=passthrough|noattr|nosys**: Runtime control of xattrs. Default is to passthrough xattr requests. 'noattr' will short circuit as if nothing exists. 'nosys' will respond with ENOSYS as if xattrs are not supported or disabled. (default: passthrough)
* **link_cow=BOOL**: When enabled if a regular file is opened which has a link count > 1 it will copy the file to a temporary file and rename over the original. Breaking the link and providing a basic copy-on-write function similar to cow-shell. (default: false)
* **statfs=base|full**: Controls how statfs works. 'base' means it will always use all branches in statfs calculations. 'full' is in effect path preserving and only includes drives where the path exists. (default: base)
* **statfs_ignore=none|ro|nc**: 'ro' will cause statfs calculations to ignore available space for branches mounted or tagged as 'read-only' or 'no create'. 'nc' will ignore available space for branches tagged as 'no create'. (default: none)
* **posix_acl=BOOL**: Enable POSIX ACL support (if supported by kernel and underlying filesystem). (default: false)
* **async_read=BOOL**: Perform reads asynchronously. If disabled or unavailable the kernel will ensure there is at most one pending read request per file handle and will attempt to order requests by offset. (default: true)
* **fuse_msg_size=INT**: Set the max number of pages per FUSE message. Only available on Linux >= 4.20 and ignored otherwise. (min: 1; max: 256; default: 256)
* **threads=INT**: Number of threads to use in multithreaded mode. When set to zero it will attempt to discover and use the number of logical cores. If the lookup fails it will fall back to using 4. If the thread count is set negative it will look up the number of cores then divide by the absolute value. ie. threads=-2 on an 8 core machine will result in 8 / 2 = 4 threads. There will always be at least 1 thread. NOTE: higher number of threads increases parallelism but usually decreases throughput. (default: 0)
* **fsname=STR**: Sets the name of the filesystem as seen in **mount**, **df**, etc. Defaults to a list of the source paths concatenated together with the longest common prefix removed.
* **func.FUNC=POLICY**: Sets the specific FUSE function's policy. See below for the list of value types. Example: **func.getattr=newest**
* **category.CATEGORY=POLICY**: Sets policy of all FUSE functions in the provided category. Example: **category.create=mfs**
* **cache.open=INT**: 'open' policy cache timeout in seconds. (default: 0)
* **cache.statfs=INT**: 'statfs' cache timeout in seconds. (default: 0)
* **cache.attr=INT**: File attribute cache timeout in seconds. (default: 1)
* **cache.entry=INT**: File name lookup cache timeout in seconds. (default: 1)
* **cache.negative_entry=INT**: Negative file name lookup cache timeout in seconds. (default: 0)
* **cache.files=libfuse|off|partial|full|auto-full**: File page caching mode (default: libfuse)
* **cache.symlinks=BOOL**: Cache symlinks (if supported by kernel) (default: false)
* **cache.readdir=BOOL**: Cache readdir (if supported by kernel) (default: false)
* **direct_io**: deprecated - Bypass page cache. Use `cache.files=off` instead. (default: false)
* **kernel_cache**: deprecated - Do not invalidate data cache on file open. Use `cache.files=full` instead. (default: false)
* **auto_cache**: deprecated - Invalidate data cache if file mtime or size change. Use `cache.files=auto-full` instead. (default: false)
* **async_read**: deprecated - Perform reads asynchronously. Use `async_read=true` instead.
* **sync_read**: deprecated - Perform reads synchronously. Use `async_read=false` instead.


**NOTE:** Options are evaluated in the order listed so if the options are **func.rmdir=rand,category.action=ff** the **action** category setting will override the **rmdir** setting.


#### Value Types

* BOOL = 'true' | 'false'
* INT = [0,MAX_INT]
* SIZE = 'NNM'; NN = INT, M = 'K' | 'M' | 'G' | 'T'
* STR = string
* FUNC = FUSE function
* CATEGORY = FUSE function category
* POLICY = mergerfs function policy


### branches

The 'branches' (formerly 'srcmounts') argument is a colon (':') delimited list of paths to be pooled together. It does not matter if the paths are on the same or different drives nor does it matter the filesystem. Used and available space will not be duplicated for paths on the same device and any features which aren't supported by the underlying filesystem (such as file attributes or extended attributes) will return the appropriate errors.

To make it easier to include multiple branches mergerfs supports [globbing](http://linux.die.net/man/7/glob). **The globbing tokens MUST be escaped when using via the shell else the shell itself will apply the glob itself.**

Each branch can have a suffix of `=RW` (read / write), `=RO` (read-only), or `=NC` (no create). These suffixes work with globs as well and will apply to each path found. `RW` is the default behavior and those paths will be eligible for all policy categories. `RO` will exclude those paths from `create` and `action` policies (just as a filesystem being mounted `ro` would). `NC` will exclude those paths from `create` policies (you can't create but you can change / delete).

```
# mergerfs -o allow_other,use_ino /mnt/disk\*:/mnt/cdrom /media/drives
```

The above line will use all mount points in /mnt prefixed with **disk** and the **cdrom**.

To have the pool mounted at boot or otherwise accessable from related tools use **/etc/fstab**.

```
# <file system>        <mount point>  <type>         <options>             <dump>  <pass>
/mnt/disk*:/mnt/cdrom  /media/drives  fuse.mergerfs  allow_other,use_ino   0       0
```

**NOTE:** the globbing is done at mount or xattr update time (see below). If a new directory is added matching the glob after the fact it will not be automatically included.

**NOTE:** for mounting via **fstab** to work you must have **mount.fuse** installed. For Ubuntu/Debian it is included in the **fuse** package.


### fuse_msg_size

FUSE applications communicate with the kernel over a special character device: `/dev/fuse`. A large portion of the overhead associated with FUSE is the cost of going back and forth from user space and kernel space over that device. Generally speaking the fewer trips needed the better the performance will be. Reducing the number of trips can be done a number of ways. Kernel level caching and increasing message sizes being two significant ones. When it comes to reads and writes if the message size is doubled the number of trips are appoximately halved.

In Linux 4.20 a new feature was added allowing the negotiation of the max message size. Since the size is in multiples of [pages](https://en.wikipedia.org/wiki/Page_(computer_memory)) the feature is called `max_pages`. There is a maximum `max_pages` value of 256 (1MiB) and minimum of 1 (4KiB). The default used by Linux >=4.20, and hardcoded value used before 4.20, is 32 (128KiB). In mergerfs its referred to as `fuse_msg_size` to make it clear what it impacts and provide some abstraction.

Since there should be no downsides to increasing `fuse_msg_size` / `max_pages`, outside a minor bump in RAM usage due to larger message buffers, mergerfs defaults the value to 256. On kernels before 4.20 the value has no effect. The reason the value is configurable is to enable experimentation and benchmarking. See the `nullrw` section for benchmarking examples.


### symlinkify

Due to the levels of indirection introduced by mergerfs and the underlying technology FUSE there can be varying levels of performance degredation. This feature will turn non-directories which are not writable into symlinks to the original file found by the `readlink` policy after the mtime and ctime are older than the timeout.

**WARNING:** The current implementation has a known issue in which if the file is open and being used when the file is converted to a symlink then the application which has that file open will receive an error when using it. This is unlikely to occur in practice but is something to keep in mind.

**WARNING:** Some backup solutions, such as CrashPlan, do not backup the target of a symlink. If using this feature it will be necessary to point any backup software to the original drives or configure the software to follow symlinks if such an option is available. Alternatively create two mounts. One for backup and one for general consumption.


### nullrw

Due to how FUSE works there is an overhead to all requests made to a FUSE filesystem. Meaning that even a simple passthrough will have some slowdown. However, generally the overhead is minimal in comparison to the cost of the underlying I/O. By disabling the underlying I/O we can test the theoretical performance boundries.

By enabling `nullrw` mergerfs will work as it always does **except** that all reads and writes will be no-ops. A write will succeed (the size of the write will be returned as if it were successful) but mergerfs does nothing with the data it was given. Similarly a read will return the size requested but won't touch the buffer.

Example:
```
$ dd if=/dev/zero of=/path/to/mergerfs/mount/benchmark ibs=1M obs=512 count=1024 iflag=dsync,nocache oflag=dsync,nocache conv=fdatasync status=progress
1024+0 records in
2097152+0 records out
1073741824 bytes (1.1 GB, 1.0 GiB) copied, 15.4067 s, 69.7 MB/s

$ dd if=/dev/zero of=/path/to/mergerfs/mount/benchmark ibs=1M obs=1M count=1024 iflag=dsync,nocache oflag=dsync,nocache conv=fdatasync status=progress
1024+0 records in
1024+0 records out
1073741824 bytes (1.1 GB, 1.0 GiB) copied, 0.219585 s, 4.9 GB/s

$ dd if=/path/to/mergerfs/mount/benchmark of=/dev/null bs=512 count=102400 iflag=dsync,nocache oflag=dsync,nocache conv=fdatasync status=progress
102400+0 records in
102400+0 records out
52428800 bytes (52 MB, 50 MiB) copied, 0.757991 s, 69.2 MB/s

$ dd if=/path/to/mergerfs/mount/benchmark of=/dev/null bs=1M count=1024 iflag=dsync,nocache oflag=dsync,nocache conv=fdatasync status=progress
1024+0 records in
1024+0 records out
1073741824 bytes (1.1 GB, 1.0 GiB) copied, 0.18405 s, 5.8 GB/s
```

It's important to test with different `obs` (output block size) values since the relative overhead is greater with smaller values. As you can see above the size of a read or write can massively impact theoretical performance. If an application performs much worse through mergerfs it could very well be that it doesn't optimally size its read and write requests. In such cases contact the mergerfs author so it can be investigated.


### xattr

Runtime extended attribute support can be managed via the `xattr` option. By default it will passthrough any xattr calls. Given xattr support is rarely used and can have significant performance implications mergerfs allows it to be disabled at runtime.

`noattr` will cause mergerfs to short circuit all xattr calls and return ENOATTR where appropriate. mergerfs still gets all the requests but they will not be forwarded on to the underlying filesystems. The runtime control will still function in this mode.

`nosys` will cause mergerfs to return ENOSYS for any xattr call. The difference with `noattr` is that the kernel will cache this fact and itself short circuit future calls. This will be more efficient than `noattr` but will cause mergerfs' runtime control via the hidden file to stop working.


# FUNCTIONS / POLICIES / CATEGORIES

The POSIX filesystem API is made up of a number of functions. **creat**, **stat**, **chown**, etc. In mergerfs most of the core functions are grouped into 3 categories: **action**, **create**, and **search**. These functions and categories can be assigned a policy which dictates what file or directory is chosen when performing that behavior. Any policy can be assigned to a function or category though some may not be very useful in practice. For instance: **rand** (random) may be useful for file creation (create) but could lead to very odd behavior if used for `chmod` if there were more than one copy of the file.

Some functions, listed in the category `N/A` below, can not be assigned the normal policies. All functions which work on file handles use the handle which was acquired by `open` or `create`. `readdir` has no real need for a policy given the purpose is merely to return a list of entries in a directory. `statfs`'s behavior can be modified via other options. That said many times the current FUSE kernel driver will not always provide the file handle when a client calls `fgetattr`, `fchown`, `fchmod`, `futimens`, `ftruncate`, etc. This means it will call the regular, path based, versions.

When using policies which are based on a branch's available space the base path provided is used. Not the full path to the file in question. Meaning that sub mounts won't be considered in the space calculations. The reason is that it doesn't really work for non-path preserving policies and can lead to non-obvious behaviors.

#### Function / Category classifications

| Category | FUSE Functions                                                                      |
|----------|-------------------------------------------------------------------------------------|
| action   | chmod, chown, link, removexattr, rename, rmdir, setxattr, truncate, unlink, utimens |
| create   | create, mkdir, mknod, symlink                                                       |
| search   | access, getattr, getxattr, ioctl (directories), listxattr, open, readlink                         |
| N/A      | fchmod, fchown, futimens, ftruncate, fallocate, fgetattr, fsync, ioctl (files), read, readdir, release, statfs, write, copy_file_range |

In cases where something may be searched (to confirm a directory exists across all source mounts) **getattr** will be used.


#### Path Preservation

Policies, as described below, are of two basic types. `path preserving` and `non-path preserving`.

All policies which start with `ep` (**epff**, **eplfs**, **eplus**, **epmfs**, **eprand**) are `path preserving`. `ep` stands for `existing path`.

A path preserving policy will only consider drives where the relative path being accessed already exists.

When using non-path preserving policies paths will be cloned to target drives as necessary.


#### Filters

Policies basically search branches and create a list of files / paths for functions to work on. The policy is responsible for filtering and sorting. The policy type defines the sorting but filtering is mostly uniform as described below.

* No **search** policies filter.
* All **action** policies will filter out branches which are mounted **read-only** or tagged as **RO (read-only)**.
* All **create** policies will filter out branches which are mounted **read-only**, tagged **RO (read-only)** or **NC (no create)**, or has available space less than `minfreespace`.

If all branches are filtered an error will be returned. Typically **EROFS** or **ENOSPC** depending on the reasons.


#### Policy descriptions

| Policy           | Description                                                |
|------------------|------------------------------------------------------------|
| all | Search category: same as **epall**. Action category: same as **epall**. Create category: for **mkdir**, **mknod**, and **symlink** it will apply to all branches. **create** works like **ff**. |
| epall (existing path, all) | Search category: same as **epff** (but more expensive because it doesn't stop after finding a valid branch). Action category: apply to all found. Create category: for **mkdir**, **mknod**, and **symlink** it will apply to all found. **create** works like **epff** (but more expensive because it doesn't stop after finding a valid branch). |
| epff (existing path, first found) | Given the order of the branches, as defined at mount time or configured at runtime, act on the first one found where the relative path exists. |
| eplfs (existing path, least free space) | Of all the branches on which the relative path exists choose the drive with the least free space. |
| eplus (existing path, least used space) | Of all the branches on which the relative path exists choose the drive with the least used space. |
| epmfs (existing path, most free space) | Of all the branches on which the relative path exists choose the drive with the most free space. |
| eprand (existing path, random) | Calls **epall** and then randomizes. |
| erofs | Exclusively return **-1** with **errno** set to **EROFS** (read-only filesystem). |
| ff (first found) | Search category: same as **epff**. Action category: same as **epff**. Create category: Given the order of the drives, as defined at mount time or configured at runtime, act on the first one found. |
| lfs (least free space) | Search category: same as **eplfs**. Action category: same as **eplfs**. Create category: Pick the drive with the least available free space. |
| lus (least used space) | Search category: same as **eplus**. Action category: same as **eplus**. Create category: Pick the drive with the least used space. |
| mfs (most free space) | Search category: same as **epmfs**. Action category: same as **epmfs**. Create category: Pick the drive with the most available free space. |
| newest | Pick the file / directory with the largest mtime. |
| rand (random) | Calls **all** and then randomizes. |


#### Defaults ####

| Category | Policy |
|----------|--------|
| action   | epall  |
| create   | epmfs  |
| search   | ff     |


#### ioctl

When `ioctl` is used with an open file then it will use the file handle which was created at the original `open` call. However, when using `ioctl` with a directory mergerfs will use the `open` policy to find the directory to act on.


#### unlink

In FUSE there is an opaque "file handle" which is created by `open`, `create`, or `opendir`, passed to the kernel, and then is passed back to the FUSE userland application by the kernel. Unfortunately, the FUSE kernel driver does not always send the file handle when it theoretically could/should. This complicates certain behaviors / workflows particularly in the high level API. As a result mergerfs is currently doing a few hacky things.

libfuse2 and libfuse3, when using the high level API, will rename names to `.fuse_hiddenXXXXXX` if the file is open when unlinked or renamed over. It does this so the file is still available when a request referencing the now missing file is made. This file however keeps a `rmdir` from succeeding and can be picked up by software reading directories.

The change mergerfs has done is that if a file is open when an unlink or rename happens it will open the file and keep it open till closed by all those who opened it prior. When a request comes in referencing that file and it doesn't include a file handle it will instead use the file handle created at unlink/rename time.

This won't result in technically proper behavior but close enough for many usecases.

The plan is to rewrite mergerfs to use the low level API so these invasive libfuse changes are no longer necessary.


#### rename & link ####

**NOTE:** If you're receiving errors from software when files are moved / renamed / linked then you should consider changing the create policy to one which is **not** path preserving, enabling `ignorepponrename`, or contacting the author of the offending software and requesting that `EXDEV` be properly handled.

`rename` and `link` are tricky functions in a union filesystem. `rename` only works within a single filesystem or device. If a rename can't be done atomically due to the source and destination paths existing on different mount points it will return **-1** with **errno = EXDEV** (cross device). So if a `rename`'s source and target are on different drives within the pool it creates an issue.

Originally mergerfs would return EXDEV whenever a rename was requested which was cross directory in any way. This made the code simple and was technically complient with POSIX requirements. However, many applications fail to handle EXDEV at all and treat it as a normal error or otherwise handle it poorly. Such apps include: gvfsd-fuse v1.20.3 and prior, Finder / CIFS/SMB client in Apple OSX 10.9+, NZBGet, Samba's recycling bin feature.

As a result a compromise was made in order to get most software to work while still obeying mergerfs' policies. Below is the basic logic.

* If using a **create** policy which tries to preserve directory paths (epff,eplfs,eplus,epmfs)
  * Using the **rename** policy get the list of files to rename
  * For each file attempt rename:
    * If failure with ENOENT run **create** policy
    * If create policy returns the same drive as currently evaluating then clone the path
    * Re-attempt rename
  * If **any** of the renames succeed the higher level rename is considered a success
  * If **no** renames succeed the first error encountered will be returned
  * On success:
    * Remove the target from all drives with no source file
    * Remove the source from all drives which failed to rename
* If using a **create** policy which does **not** try to preserve directory paths
  * Using the **rename** policy get the list of files to rename
  * Using the **getattr** policy get the target path
  * For each file attempt rename:
    * If the source drive != target drive:
      * Clone target path from target drive to source drive
    * Rename
  * If **any** of the renames succeed the higher level rename is considered a success
  * If **no** renames succeed the first error encountered will be returned
  * On success:
    * Remove the target from all drives with no source file
    * Remove the source from all drives which failed to rename

The the removals are subject to normal entitlement checks.

The above behavior will help minimize the likelihood of EXDEV being returned but it will still be possible.

**link** uses the same strategy but without the removals.


#### readdir ####

[readdir](http://linux.die.net/man/3/readdir) is different from all other filesystem functions. While it could have its own set of policies to tweak its behavior at this time it provides a simple union of files and directories found. Remember that any action or information queried about these files and directories come from the respective function. For instance: an **ls** is a **readdir** and for each file/directory returned **getattr** is called. Meaning the policy of **getattr** is responsible for choosing the file/directory which is the source of the metadata you see in an **ls**.


#### statfs / statvfs ####

[statvfs](http://linux.die.net/man/2/statvfs) normalizes the source drives based on the fragment size and sums the number of adjusted blocks and inodes. This means you will see the combined space of all sources. Total, used, and free. The sources however are dedupped based on the drive so multiple sources on the same drive will not result in double counting its space. Filesystems mounted further down the tree of the branch will not be included when checking the mount's stats.

The options `statfs` and `statfs_ignore` can be used to modify `statfs` behavior.


# BUILDING

**NOTE:** Prebuilt packages can be found at: https://github.com/trapexit/mergerfs/releases

First get the code from [github](https://github.com/trapexit/mergerfs).

```
$ git clone https://github.com/trapexit/mergerfs.git
$ # or
$ wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-<ver>.tar.gz
```

#### Debian / Ubuntu
```
$ cd mergerfs
$ sudo tools/install-build-pkgs
$ make deb
$ sudo dpkg -i ../mergerfs_version_arch.deb
```

#### Fedora
```
$ su -
# cd mergerfs
# tools/install-build-pkgs
# make rpm
# rpm -i rpmbuild/RPMS/<arch>/mergerfs-<verion>.<arch>.rpm
```

#### Generically

Have git, g++, make, python installed.

```
$ cd mergerfs
$ make
$ sudo make install
```

#### Build options

```
$ make help
usage: make

make USE_XATTR=0      - build program without xattrs functionality
make STATIC=1         - build static binary
make LTO=1            - build with link time optimization
```


# RUNTIME CONFIG

#### .mergerfs pseudo file ####

```
<mountpoint>/.mergerfs
```

There is a pseudo file available at the mount point which allows for the runtime modification of certain **mergerfs** options. The file will not show up in **readdir** but can be **stat**'ed and manipulated via [{list,get,set}xattrs](http://linux.die.net/man/2/listxattr) calls.

Any changes made at runtime are **not** persisted. If you wish for values to persist they must be included as options wherever you configure the mounting of mergerfs (/etc/fstab).


##### Keys #####

Use `xattr -l /mountpoint/.mergerfs` to see all supported keys. Some are informational and therefore read-only. `setxattr` will return EINVAL on read-only keys.


##### Values #####

Same as the command line.


###### user.mergerfs.branches ######

**NOTE:** formerly `user.mergerfs.srcmounts` but said key is still supported.

Used to query or modify the list of branches. When modifying there are several shortcuts to easy manipulation of the list.

| Value        | Description |
|--------------|-------------|
| [list]       | set         |
| +<[list]     | prepend     |
| +>[list]     | append      |
| -[list]      | remove all values provided |
| -<           | remove first in list |
| ->           | remove last in list |

`xattr -w user.mergerfs.branches +</mnt/drive3 /mnt/pool/.mergerfs`

The `=NC`, `=RO`, `=RW` syntax works just as on the command line.


##### Example #####

```
[trapexit:/mnt/mergerfs] $ xattr -l .mergerfs
user.mergerfs.branches: /mnt/a=RW:/mnt/b=RW
user.mergerfs.minfreespace: 4294967295
user.mergerfs.moveonenospc: false
...

[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.category.search .mergerfs
ff

[trapexit:/mnt/mergerfs] $ xattr -w user.mergerfs.category.search newest .mergerfs
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.category.search .mergerfs
newest

[trapexit:/mnt/mergerfs] $ xattr -w user.mergerfs.branches +/mnt/c .mergerfs
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.branches .mergerfs
/mnt/a:/mnt/b:/mnt/c

[trapexit:/mnt/mergerfs] $ xattr -w user.mergerfs.branches =/mnt/c .mergerfs
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.branches .mergerfs
/mnt/c

[trapexit:/mnt/mergerfs] $ xattr -w user.mergerfs.branches '+</mnt/a:/mnt/b' .mergerfs
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.branches .mergerfs
/mnt/a:/mnt/b:/mnt/c
```


#### file / directory xattrs ####

While they won't show up when using [listxattr](http://linux.die.net/man/2/listxattr) **mergerfs** offers a number of special xattrs to query information about the files served. To access the values you will need to issue a [getxattr](http://linux.die.net/man/2/getxattr) for one of the following:

* **user.mergerfs.basepath**: the base mount point for the file given the current getattr policy
* **user.mergerfs.relpath**: the relative path of the file from the perspective of the mount point
* **user.mergerfs.fullpath**: the full path of the original file given the getattr policy
* **user.mergerfs.allpaths**: a NUL ('\0') separated list of full paths to all files found

```
[trapexit:/mnt/mergerfs] $ ls
A B C
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.fullpath A
/mnt/a/full/path/to/A
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.basepath A
/mnt/a
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.relpath A
/full/path/to/A
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.allpaths A | tr '\0' '\n'
/mnt/a/full/path/to/A
/mnt/b/full/path/to/A
```


# TOOLING

* https://github.com/trapexit/mergerfs-tools
  * mergerfs.ctl: A tool to make it easier to query and configure mergerfs at runtime
  * mergerfs.fsck: Provides permissions and ownership auditing and the ability to fix them
  * mergerfs.dedup: Will help identify and optionally remove duplicate files
  * mergerfs.dup: Ensure there are at least N copies of a file across the pool
  * mergerfs.balance: Rebalance files across drives by moving them from the most filled to the least filled
  * mergerfs.mktrash: Creates FreeDesktop.org Trash specification compatible directories on a mergerfs mount
* https://github.com/trapexit/scorch
  * scorch: A tool to help discover silent corruption of files and keep track of files
* https://github.com/trapexit/bbf
  * bbf (bad block finder): a tool to scan for and 'fix' hard drive bad blocks and find the files using those blocks


# CACHING

#### page caching

https://en.wikipedia.org/wiki/Page_cache

tl;dr:
* cache.files=off: Disables page caching. Underlying files cached, mergerfs files are not.
* cache.files=partial: Enables page caching. Underlying files cached, mergerfs files cached while open.
* cache.files=full: Enables page caching. Underlying files cached, mergerfs files cached across opens.
* cache.files=auto-full: Enables page caching. Underlying files cached, mergerfs files cached across opens if mtime and size are unchanged since previous open.
* cache.files=libfuse: follow traditional libfuse `direct_io`, `kernel_cache`, and `auto_cache` arguments.


FUSE, which mergerfs uses, offers a number of page caching modes. mergerfs tries to simplify their use via the `cache.files` option. It can and should replace usage of `direct_io`, `kernel_cache`, and `auto_cache`.

Due to mergerfs using FUSE and therefore being a userland process proxying existing filesystems the kernel will double cache the content being read and written through mergerfs. Once from the underlying filesystem and once from mergerfs (it sees them as two separate entities). Using `cache.files=off` will keep the double caching from happening by disabling caching of mergerfs but this has the side effect that *all* read and write calls will be passed to mergerfs which may be slower than enabling caching, you lose shared `mmap` support which can affect apps such as rtorrent, and no read-ahead will take place. The kernel will still cache the underlying filesystem data but that only helps so much given mergerfs will still process all requests.

If you do enable file page caching, `cache.files=partial|full|auto-full`, you should also enable `dropcacheonclose` which will cause mergerfs to instruct the kernel to flush the underlying file's page cache when the file is closed. This behavior is the same as the rsync fadvise / drop cache patch and Feh's nocache project.

If most files are read once through and closed (like media) it is best to enable `dropcacheonclose` regardless of caching mode in order to minimize buffer bloat.

It is difficult to balance memory usage, cache bloat & duplication, and performance. Ideally mergerfs would be able to disable caching for the files it reads/writes but allow page caching for itself. That would limit the FUSE overhead. However, there isn't a good way to achieve this. It would need to open all files with O_DIRECT which places limitations on the what underlying filesystems would be supported and complicates the code.

kernel documenation: https://www.kernel.org/doc/Documentation/filesystems/fuse-io.txt


#### entry & attribute caching

Given the relatively high cost of FUSE due to the kernel <-> userspace round trips there are kernel side caches for file entries and attributes. The entry cache limits the `lookup` calls to mergerfs which ask if a file exists. The attribute cache limits the need to make `getattr` calls to mergerfs which provide file attributes (mode, size, type, etc.). As with the page cache these should not be used if the underlying filesystems are being manipulated at the same time as it could lead to odd behavior or data corruption. The options for setting these are `cache.entry` and `cache.negative_entry` for the entry cache and `cache.attr` for the attributes cache. `cache.negative_entry` refers to the timeout for negative responses to lookups (non-existant files).


#### policy caching

Policies are run every time a function (with a policy as mentioned above) is called. These policies can be expensive depending on mergerfs' setup and client usage patterns. Generally we wouldn't want to cache policy results because it may result in stale responses if the underlying drives are used directly.

The `open` policy cache will cache the result of an `open` policy for a particular input for `cache.open` seconds or until the file is unlinked. Each file close (release) will randomly chose to clean up the cache of expired entries.

This cache is really only useful in cases where you have a large number of branches and `open` is called on the same files repeatedly (like **Transmission** which opens and closes a file on every read/write presumably to keep file handle usage low).


#### statfs caching

Of the syscalls used by mergerfs in policies the `statfs` / `statvfs` call is perhaps the most expensive. It's used to find out the available space of a drive and whether it is mounted read-only. Depending on the setup and usage pattern these queries can be relatively costly. When `cache.statfs` is enabled all calls to `statfs` by a policy will be cached for the number of seconds its set to.

Example: If the create policy is `mfs` and the timeout is 60 then for that 60 seconds the same drive will be returned as the target for creates because the available space won't be updated for that time.


#### symlink caching

As of version 4.20 Linux supports symlink caching. Significant performance increases can be had in workloads which use a lot of symlinks. Setting `cache.symlinks=true` will result in requesting symlink caching from the kernel only if supported. As a result its safe to enable it on systems prior to 4.20. That said it is disabled by default for now. You can see if caching is enabled by querying the xattr `user.mergerfs.cache.symlinks` but given it must be requested at startup you can not change it at runtime.


#### readdir caching

As of version 4.20 Linux supports readdir caching. This can have a significant impact on directory traversal. Especially when combined with entry (`cache.entry`) and attribute (`cache.attr`) caching. Setting `cache.readdir=true` will result in requesting readdir caching from the kernel on each `opendir`. If the kernel doesn't support readdir caching setting the option to `true` has no effect. This option is configuarable at runtime via xattr `user.mergerfs.cache.readdir`.


#### writeback caching

writeback caching is a technique for improving write speeds by batching writes at a faster device and then bulk writing to the slower device. With FUSE the kernel will wait for a number of writes to be made and then send it to the filesystem as one request. mergerfs currently uses a  modified and vendored libfuse 2.9.7 which does not support writeback caching. Adding said feature should not be difficult but benchmarking needs to be done to see if what effect it will have.


#### tiered caching

Some storage technologies support what some call "tiered" caching. The placing of usually smaller, faster storage as a transparent cache to larger, slower storage. NVMe, SSD, Optane in front of traditional HDDs for instance.

MergerFS does not natively support any sort of tiered caching. Most users have no use for such a feature and its inclusion would complicate the code. However, there are a few situations where a cache drive could help with a typical mergerfs setup.

1. Fast network, slow drives, many readers: You've a 10+Gbps network with many readers and your regular drives can't keep up.
2. Fast network, slow drives, small'ish bursty writes: You have a 10+Gbps network and wish to transfer amounts of data less than your cache drive but wish to do so quickly.

With #1 its arguable if you should be using mergerfs at all. RAID would probably be the better solution. If you're going to use mergerfs there are other tactics that may help: spreading the data across drives (see the mergerfs.dup tool) and setting `func.open=rand`, using `symlinkify`, or using dm-cache or a similar technology to add tiered cache to the underlying device.

With #2 one could use dm-cache as well but there is another solution which requires only mergerfs and a cronjob.

1. Create 2 mergerfs pools. One which includes just the slow drives and one which has both the fast drives (SSD,NVME,etc.) and slow drives.
2. The 'cache' pool should have the cache drives listed first.
3. The best `create` policies to use for the 'cache' pool would probably be `ff`, `epff`, `lfs`, or `eplfs`. The latter two under the assumption that the cache drive(s) are far smaller than the backing drives. If using path preserving policies remember that you'll need to manually create the core directories of those paths you wish to be cached. Be sure the permissions are in sync. Use `mergerfs.fsck` to check / correct them. You could also tag the slow drives as `=NC` though that'd mean if the cache drives fill you'd get "out of space" errors.
4. Enable `moveonenospc` and set `minfreespace` appropriately. Perhaps setting `minfreespace` to the size of the largest cache drive.
5. Set your programs to use the cache pool.
6. Save one of the below scripts or create you're own.
7. Use `cron` (as root) to schedule the command at whatever frequency is appropriate for your workflow.


##### time based expiring

Move files from cache to backing pool based only on the last time the file was accessed. Replace `-atime` with `-amin` if you want minutes rather than days. May want to use the `fadvise` / `--drop-cache` version of rsync or run rsync with the tool "nocache".

```
#!/bin/bash

if [ $# != 3 ]; then
  echo "usage: $0 <cache-drive> <backing-pool> <days-old>"
  exit 1
fi

CACHE="${1}"
BACKING="${2}"
N=${3}

find "${CACHE}" -type f -atime +${N} -printf '%P\n' | \
  rsync --files-from=- -axqHAXWES --preallocate --remove-source-files "${CACHE}/" "${BACKING}/"
```


##### percentage full expiring

Move the oldest file from the cache to the backing pool. Continue till below percentage threshold.

```
#!/bin/bash

if [ $# != 3 ]; then
  echo "usage: $0 <cache-drive> <backing-pool> <percentage>"
  exit 1
fi

CACHE="${1}"
BACKING="${2}"
PERCENTAGE=${3}

set -o errexit
while [ $(df --output=pcent "${CACHE}" | grep -v Use | cut -d'%' -f1) -gt ${PERCENTAGE} ]
do
    FILE=$(find "${CACHE}" -type f -printf '%A@ %P\n' | \
                  sort | \
                  head -n 1 | \
                  cut -d' ' -f2-)
    test -n "${FILE}"
    rsync -axqHAXWES --preallocate --remove-source-files "${CACHE}/./${FILE}" "${BACKING}/"
done
```


# TIPS / NOTES

* **use_ino** will only work when used with mergerfs 2.18.0 and above.
* Run mergerfs as `root` (with **allow_other**) unless you're merging paths which are owned by the same user otherwise strange permission issues may arise.
* https://github.com/trapexit/backup-and-recovery-howtos : A set of guides / howtos on creating a data storage system, backing it up, maintaining it, and recovering from failure.
* If you don't see some directories and files you expect in a merged point or policies seem to skip drives be sure the user has permission to all the underlying directories. Use `mergerfs.fsck` to audit the drive for out of sync permissions.
* Do **not** use `cache.files=off` or `direct_io` if you expect applications (such as rtorrent) to [mmap](http://linux.die.net/man/2/mmap) files. Shared mmap is not currently supported in FUSE w/ `direct_io` enabled. Enabling `dropcacheonclose` is recommended when `cache.files=partial|full|auto-full` or `direct_io=false`.
* Since POSIX functions give only a singular error or success its difficult to determine the proper behavior when applying the function to multiple targets. **mergerfs** will return an error only if all attempts of an action fail. Any success will lead to a success returned. This means however that some odd situations may arise.
* [Kodi](http://kodi.tv), [Plex](http://plex.tv), [Subsonic](http://subsonic.org), etc. can use directory [mtime](http://linux.die.net/man/2/stat) to more efficiently determine whether to scan for new content rather than simply performing a full scan. If using the default **getattr** policy of **ff** its possible those programs will miss an update on account of it returning the first directory found's **stat** info and its a later directory on another mount which had the **mtime** recently updated. To fix this you will want to set **func.getattr=newest**. Remember though that this is just **stat**. If the file is later **open**'ed or **unlink**'ed and the policy is different for those then a completely different file or directory could be acted on.
* Some policies mixed with some functions may result in strange behaviors. Not that some of these behaviors and race conditions couldn't happen outside **mergerfs** but that they are far more likely to occur on account of the attempt to merge together multiple sources of data which could be out of sync due to the different policies.
* For consistency its generally best to set **category** wide policies rather than individual **func**'s. This will help limit the confusion of tools such as [rsync](http://linux.die.net/man/1/rsync). However, the flexibility is there if needed.


# KNOWN ISSUES / BUGS

#### directory mtime is not being updated

Remember that the default policy for `getattr` is `ff`. The information for the first directory found will be returned. If it wasn't the directory which had been updated then it will appear outdated.

The reason this is the default is because any other policy would be more expensive and for many applications it is unnecessary. To always return the directory with the most recent mtime or a faked value based on all found would require a scan of all drives.

If you always want the directory information from the one with the most recent mtime then use the `newest` policy for `getattr`.


#### `mv /mnt/pool/foo /mnt/disk1/foo` removes `foo`

This is not a bug.

Run in verbose mode to better undertand what's happening:

```
$ mv -v /mnt/pool/foo /mnt/disk1/foo
copied '/mnt/pool/foo' -> '/mnt/disk1/foo'
removed '/mnt/pool/foo'
$ ls /mnt/pool/foo
ls: cannot access '/mnt/pool/foo': No such file or directory
```

`mv`, when working across devices, is copying the source to target and then removing the source. Since the source **is** the target in this case, depending on the unlink policy, it will remove the just copied file and other files across the branches.

If you want to move files to one drive just copy them there and use mergerfs.dedup to clean up the old paths or manually remove them from the branches directly.


#### cached memory appears greater than it should be

Use `cache.files=off` or `direct_io=true`. See the section on page caching.


#### NFS clients returning ESTALE / Stale file handle

Be sure to use `noforget` and `use_ino` arguments.


#### NFS clients don't work

Some NFS clients appear to fail when a mergerfs mount is exported. Kodi in particular seems to have issues.

Try enabling the `use_ino` option. Some have reported that it fixes the issue.


#### rtorrent fails with ENODEV (No such device)

Be sure to set `cache.files=partial|full|auto-full` or turn off `direct_io`. rtorrent and some other applications use [mmap](http://linux.die.net/man/2/mmap) to read and write to files and offer no failback to traditional methods. FUSE does not currently support mmap while using `direct_io`. There may be a performance penalty on writes with `direct_io` off as well as the problem of double caching but it's the only way to get such applications to work. If the performance loss is too high for other apps you can mount mergerfs twice. Once with `direct_io` enabled and one without it. Be sure to set `dropcacheonclose=true` if not using `direct_io`.


#### rtorrent fails with files >= 4GiB

This is a kernel bug with mmap and FUSE on 32bit platforms. A fix should become available for all LTS releases.

https://marc.info/?l=linux-fsdevel&m=155550785230874&w=2


#### Plex doesn't work with mergerfs

It does. If you're trying to put Plex's config / metadata on mergerfs you have to leave `direct_io` off because Plex is using sqlite which apparently needs mmap. mmap doesn't work with `direct_io`. To fix this place the data elsewhere or disable `direct_io` (with `dropcacheonclose=true`).

If the issue is that scanning doesn't seem to pick up media then be sure to set `func.getattr=newest` as mentioned above.


#### mmap performance is really bad

There [is a bug](https://lkml.org/lkml/2016/3/16/260) in caching which affects overall performance of mmap through FUSE in Linux 4.x kernels. It is fixed in [4.4.10 and 4.5.4](https://lkml.org/lkml/2016/5/11/59).


#### When a program tries to move or rename a file it fails

Please read the section above regarding [rename & link](#rename--link).

The problem is that many applications do not properly handle `EXDEV` errors which `rename` and `link` may return even though they are perfectly valid situations which do not indicate actual drive or OS errors. The error will only be returned by mergerfs if using a path preserving policy as described in the policy section above. If you do not care about path preservation simply change the mergerfs policy to the non-path preserving version. For example: `-o category.create=mfs`

Ideally the offending software would be fixed and it is recommended that if you run into this problem you contact the software's author and request proper handling of `EXDEV` errors.


#### Samba: Moving files / directories fails

Workaround: Copy the file/directory and then remove the original rather than move.

This isn't an issue with Samba but some SMB clients. GVFS-fuse v1.20.3 and prior (found in Ubuntu 14.04 among others) failed to handle certain error codes correctly. Particularly **STATUS_NOT_SAME_DEVICE** which comes from the **EXDEV** which is returned by **rename** when the call is crossing mount points. When a program gets an **EXDEV** it needs to explicitly take an alternate action to accomplish its goal. In the case of **mv** or similar it tries **rename** and on **EXDEV** falls back to a manual copying of data between the two locations and unlinking the source. In these older versions of GVFS-fuse if it received **EXDEV** it would translate that into **EIO**. This would cause **mv** or most any application attempting to move files around on that SMB share to fail with a IO error.

[GVFS-fuse v1.22.0](https://bugzilla.gnome.org/show_bug.cgi?id=734568) and above fixed this issue but a large number of systems use the older release. On Ubuntu the version can be checked by issuing `apt-cache showpkg gvfs-fuse`. Most distros released in 2015 seem to have the updated release and will work fine but older systems may not. Upgrading gvfs-fuse or the distro in general will address the problem.

In Apple's MacOSX 10.9 they replaced Samba (client and server) with their own product. It appears their new client does not handle **EXDEV** either and responds similar to older release of gvfs on Linux.


#### Trashing files occasionally fails

This is the same issue as with Samba. `rename` returns `EXDEV` (in our case that will really only happen with path preserving policies like `epmfs`) and the software doesn't handle the situtation well. This is unfortunately a common failure of software which moves files around. The standard indicates that an implementation `MAY` choose to support non-user home directory trashing of files (which is a `MUST`). The implementation `MAY` also support "top directory trashes" which many probably do.

To create a `$topdir/.Trash` directory as defined in the standard use the [mergerfs-tools](https://github.com/trapexit/mergerfs-tools) tool `mergerfs.mktrash`.

#### tar: Directory renamed before its status could be extracted

Make sure to use the `use_ino` option.


#### Supplemental user groups

Due to the overhead of [getgroups/setgroups](http://linux.die.net/man/2/setgroups) mergerfs utilizes a cache. This cache is opportunistic and per thread. Each thread will query the supplemental groups for a user when that particular thread needs to change credentials and will keep that data for the lifetime of the thread. This means that if a user is added to a group it may not be picked up without the restart of mergerfs. However, since the high level FUSE API's (at least the standard version) thread pool dynamically grows and shrinks it's possible that over time a thread will be killed and later a new thread with no cache will start and query the new data.

The gid cache uses fixed storage to simplify the design and be compatible with older systems which may not have C++11 compilers. There is enough storage for 256 users' supplemental groups. Each user is allowed upto 32 supplemental groups. Linux >= 2.6.3 allows upto 65535 groups per user but most other *nixs allow far less. NFS allowing only 16. The system does handle overflow gracefully. If the user has more than 32 supplemental groups only the first 32 will be used. If more than 256 users are using the system when an uncached user is found it will evict an existing user's cache at random. So long as there aren't more than 256 active users this should be fine. If either value is too low for your needs you will have to modify `gidcache.hpp` to increase the values. Note that doing so will increase the memory needed by each thread.


#### mergerfs or libfuse crashing

**NOTE:** as of mergerfs 2.22.0 it includes the most recent version of libfuse (or requires libfuse-2.9.7) so any crash should be reported. For older releases continue reading...

If suddenly the mergerfs mount point disappears and `Transport endpoint is not connected` is returned when attempting to perform actions within the mount directory **and** the version of libfuse (use `mergerfs -v` to find the version) is older than `2.9.4` its likely due to a bug in libfuse. Affected versions of libfuse can be found in Debian Wheezy, Ubuntu Precise and others.

In order to fix this please install newer versions of libfuse. If using a Debian based distro (Debian,Ubuntu,Mint) you can likely just install newer versions of [libfuse](https://packages.debian.org/unstable/libfuse2) and [fuse](https://packages.debian.org/unstable/fuse) from the repo of a newer release.


#### mergerfs appears to be crashing or exiting

There seems to be an issue with Linux version `4.9.0` and above in which an invalid message appears to be transmitted to libfuse (used by mergerfs) causing it to exit. No messages will be printed in any logs as its not a proper crash. Debugging of the issue is still ongoing and can be followed via the [fuse-devel thread](https://sourceforge.net/p/fuse/mailman/message/35662577).

#### mergerfs under heavy load and memory preasure leads to kernel panic

https://lkml.org/lkml/2016/9/14/527

```
[25192.515454] kernel BUG at /build/linux-a2WvEb/linux-4.4.0/mm/workingset.c:346!
[25192.517521] invalid opcode: 0000 [#1] SMP
[25192.519602] Modules linked in: netconsole ip6t_REJECT nf_reject_ipv6 ipt_REJECT nf_reject_ipv4 configfs binfmt_misc veth bridge stp llc nf_conntrack_ipv6 nf_defrag_ipv6 xt_conntrack ip6table_filter ip6_tables xt_multiport iptable_filter ipt_MASQUERADE nf_nat_masquerade_ipv4 xt_comment xt_nat iptable_nat nf_conntrack_ipv4 nf_defrag_ipv4 nf_nat_ipv4 nf_nat nf_conntrack xt_CHECKSUM xt_tcpudp iptable_mangle ip_tables x_tables intel_rapl x86_pkg_temp_thermal intel_powerclamp eeepc_wmi asus_wmi coretemp sparse_keymap kvm_intel ppdev kvm irqbypass mei_me 8250_fintek input_leds serio_raw parport_pc tpm_infineon mei shpchp mac_hid parport lpc_ich autofs4 drbg ansi_cprng dm_crypt algif_skcipher af_alg btrfs raid456 async_raid6_recov async_memcpy async_pq async_xor async_tx xor raid6_pq libcrc32c raid0 multipath linear raid10 raid1 i915 crct10dif_pclmul crc32_pclmul aesni_intel i2c_algo_bit aes_x86_64 drm_kms_helper lrw gf128mul glue_helper ablk_helper syscopyarea cryptd sysfillrect sysimgblt fb_sys_fops drm ahci r8169 libahci mii wmi fjes video [last unloaded: netconsole]
[25192.540910] CPU: 2 PID: 63 Comm: kswapd0 Not tainted 4.4.0-36-generic #55-Ubuntu
[25192.543411] Hardware name: System manufacturer System Product Name/P8H67-M PRO, BIOS 3904 04/27/2013
[25192.545840] task: ffff88040cae6040 ti: ffff880407488000 task.ti: ffff880407488000
[25192.548277] RIP: 0010:[<ffffffff811ba501>]  [<ffffffff811ba501>] shadow_lru_isolate+0x181/0x190
[25192.550706] RSP: 0018:ffff88040748bbe0  EFLAGS: 00010002
[25192.553127] RAX: 0000000000001c81 RBX: ffff8802f91ee928 RCX: ffff8802f91eeb38
[25192.555544] RDX: ffff8802f91ee938 RSI: ffff8802f91ee928 RDI: ffff8804099ba2c0
[25192.557914] RBP: ffff88040748bc08 R08: 000000000001a7b6 R09: 000000000000003f
[25192.560237] R10: 000000000001a750 R11: 0000000000000000 R12: ffff8804099ba2c0
[25192.562512] R13: ffff8803157e9680 R14: ffff8803157e9668 R15: ffff8804099ba2c8
[25192.564724] FS:  0000000000000000(0000) GS:ffff88041f280000(0000) knlGS:0000000000000000
[25192.566990] CS:  0010 DS: 0000 ES: 0000 CR0: 0000000080050033
[25192.569201] CR2: 00007ffabb690000 CR3: 0000000001e0a000 CR4: 00000000000406e0
[25192.571419] Stack:
[25192.573550]  ffff8804099ba2c0 ffff88039e4f86f0 ffff8802f91ee928 ffff8804099ba2c8
[25192.575695]  ffff88040748bd08 ffff88040748bc58 ffffffff811b99bf 0000000000000052
[25192.577814]  0000000000000000 ffffffff811ba380 000000000000008a 0000000000000080
[25192.579947] Call Trace:
[25192.582022]  [<ffffffff811b99bf>] __list_lru_walk_one.isra.3+0x8f/0x130
[25192.584137]  [<ffffffff811ba380>] ? memcg_drain_all_list_lrus+0x190/0x190
[25192.586165]  [<ffffffff811b9a83>] list_lru_walk_one+0x23/0x30
[25192.588145]  [<ffffffff811ba544>] scan_shadow_nodes+0x34/0x50
[25192.590074]  [<ffffffff811a0e9d>] shrink_slab.part.40+0x1ed/0x3d0
[25192.591985]  [<ffffffff811a53da>] shrink_zone+0x2ca/0x2e0
[25192.593863]  [<ffffffff811a64ce>] kswapd+0x51e/0x990
[25192.595737]  [<ffffffff811a5fb0>] ? mem_cgroup_shrink_node_zone+0x1c0/0x1c0
[25192.597613]  [<ffffffff810a0808>] kthread+0xd8/0xf0
[25192.599495]  [<ffffffff810a0730>] ? kthread_create_on_node+0x1e0/0x1e0
[25192.601335]  [<ffffffff8182e34f>] ret_from_fork+0x3f/0x70
[25192.603193]  [<ffffffff810a0730>] ? kthread_create_on_node+0x1e0/0x1e0
```

There is a bug in the kernel. A work around appears to be turning off `splice`. Don't add the `splice_*` arguments or add `no_splice_write,no_splice_move,no_splice_read`. This, however, is not guaranteed to work.


#### rm: fts_read failed: No such file or directory

NOTE: This is only relevant to mergerfs versions at or below v2.25.x and should not occur in more recent versions. See the notes on `unlink`.

Not *really* a bug. The FUSE library will move files when asked to delete them as a way to deal with certain edge cases and then later delete that file when its clear the file is no longer needed. This however can lead to two issues. One is that these hidden files are noticed by `rm -rf` or `find` when scanning directories and they may try to remove them and they might have disappeared already. There is nothing *wrong* about this happening but it can be annoying. The second issue is that a directory might not be able to removed on account of the hidden file being still there.

Using the **hard_remove** option will make it so these temporary files are not used and files are deleted immedately. That has a side effect however. Files which are unlinked and then they are still used (in certain forms) will result in an error (ENOENT).


# FAQ

#### How well does mergerfs scale? Is it "production ready?"

Users have reported running mergerfs on everything from a Raspberry Pi to dual socket Xeon systems with >20 cores. I'm aware of at least a few companies which use mergerfs in production. [Open Media Vault](https://www.openmediavault.org) includes mergerfs as its sole solution for pooling drives.


#### Can mergerfs be used with drives which already have data / are in use?

Yes. MergerFS is a proxy and does **NOT** interfere with the normal form or function of the drives / mounts / paths it manages.

MergerFS is **not** a traditional filesystem. MergerFS is **not** RAID. It does **not** manipulate the data that passes through it. It does **not** shard data across drives. It merely shards some **behavior** and aggregates others.


#### Can mergerfs be removed without affecting the data?

See the previous question's answer.


#### Do hard links work?

Yes. You need to use `use_ino` to support proper reporting of inodes.

What mergerfs does not do is fake hard links across branches.  Read the section "rename & link" for how it works.


#### Does mergerfs support CoW / copy-on-write?

Not in the sense of a filesystem like BTRFS or ZFS nor in the overlayfs or aufs sense. It does offer a [cow-shell](http://manpages.ubuntu.com/manpages/bionic/man1/cow-shell.1.html) like hard link breaking (copy to temp file then rename over original) which can be useful when wanting to save space by hardlinking duplicate files but wish to treat each name as if it were a unique and separate file.


#### Why can't I see my files / directories?

It's almost always a permissions issue. Unlike mhddfs, which runs as root and attempts to access content as such, mergerfs always changes its credentials to that of the caller. This means that if the user does not have access to a file or directory than neither will mergerfs. However, because mergerfs is creating a union of paths it may be able to read some files and directories on one drive but not another resulting in an incomplete set.

Whenever you run into a split permission issue (seeing some but not all files) try using [mergerfs.fsck](https://github.com/trapexit/mergerfs-tools) tool to check for and fix the mismatch. If you aren't seeing anything at all be sure that the basic permissions are correct. The user and group values are correct and that directories have their executable bit set. A common mistake by users new to Linux is to `chmod -R 644` when they should have `chmod -R u=rwX,go=rX`.

If using a network filesystem such as NFS, SMB, CIFS (Samba) be sure to pay close attention to anything regarding permissioning and users. Root squashing and user translation for instance has bitten a few mergerfs users. Some of these also affect the use of mergerfs from container platforms such as Docker.


#### Why is only one drive being used?

Are you using a path preserving policy? The default policy for file creation is `epmfs`. That means only the drives with the path preexisting will be considered when creating a file. If you don't care about where files and directories are created you likely shouldn't be using a path preserving policy and instead something like `mfs`.

This can be especially apparent when filling an empty pool from an external source. If you do want path preservation you'll need to perform the manual act of creating paths on the drives you want the data to land on before transfering your data. Setting `func.mkdir=epall` can simplify managing path perservation for `create`.


#### Why was libfuse embedded into mergerfs?

1. A significant number of users use mergerfs on distros with old versions of libfuse which have serious bugs. Requiring updated versions of libfuse on those distros isn't pratical (no package offered, user inexperience, etc.). The only practical way to provide a stable runtime on those systems was to "vendor" / embed the library into the project.
2. mergerfs was written to use the high level API. There are a number of limitations in the HLAPI that make certain features difficult or impossible to implement. While some of these features could be patched into newer versions of libfuse without breaking the public API some of them would require hacky code to provide backwards compatibility. While it may still be worth working with upstream to address these issues in future versions, since the library needs to be vendored for stability and compatibility reasons it is preferable / easier to modify the API. Longer term the plan is to rewrite mergerfs to use the low level API.


#### Why did support for system libfuse get removed?

See above first.

If/when mergerfs is rewritten to use the low-level API then it'll be plausible to support system libfuse but till then its simply too much work to manage the differences across the versions.


#### Why use mergerfs over mhddfs?

mhddfs is no longer maintained and has some known stability and security issues (see below). MergerFS provides a superset of mhddfs' features and should offer the same or maybe better performance.

Below is an example of mhddfs and mergerfs setup to work similarly.

`mhddfs -o mlimit=4G,allow_other /mnt/drive1,/mnt/drive2 /mnt/pool`

`mergerfs -o minfreespace=4G,allow_other,category.create=ff /mnt/drive1:/mnt/drive2 /mnt/pool`


#### Why use mergerfs over aufs?

aufs is mostly abandoned and no longer available in many distros.

While aufs can offer better peak performance mergerfs provides more configurability and is generally easier to use. mergerfs however does not offer the overlay / copy-on-write (CoW) features which aufs and overlayfs have.


#### Why use mergerfs over unionfs?

UnionFS is more like aufs then mergerfs in that it offers overlay / CoW features. If you're just looking to create a union of drives and want flexibility in file/directory placement then mergerfs offers that whereas unionfs is more for overlaying RW filesystems over RO ones.


#### Why use mergerfs over LVM/ZFS/BTRFS/RAID0 drive concatenation / striping?

With simple JBOD / drive concatenation / stripping / RAID0 a single drive failure will result in full pool failure. mergerfs performs a similar behavior without the possibility of catastrophic failure and the difficulties in recovery. Drives may fail however all other data will continue to be accessable.

When combined with something like [SnapRaid](http://www.snapraid.it) and/or an offsite backup solution you can have the flexibilty of JBOD without the single point of failure.


#### Why use mergerfs over ZFS?

MergerFS is not intended to be a replacement for ZFS. MergerFS is intended to provide flexible pooling of arbitrary drives (local or remote), of arbitrary sizes, and arbitrary filesystems. For `write once, read many` usecases such as bulk media storage. Where data integrity and backup is managed in other ways. In that situation ZFS can introduce major maintance and cost burdens as described [here](http://louwrentius.com/the-hidden-cost-of-using-zfs-for-your-home-nas.html).


#### Can drives be written to directly? Outside of mergerfs while pooled?

Yes, however its not recommended to use the same file from within the pool and from without at the same time. Especially if using caching of any kind (cache.files, cache.entry, cache.attr, cache.negative_entry, cache.symlinks, cache.readdir, etc.).


#### Why do I get an "out of space" / "no space left on device" / ENOSPC error even though there appears to be lots of space available?

First make sure you've read the sections above about policies, path preservation, branch filtering, and the options **minfreespace**, **moveonenospc**, **statfs**, and **statfs_ignore**.

mergerfs is simply presenting a union of the content within multiple branches. The reported free space is an aggregate of space available within the pool (behavior modified by **statfs** and **statfs_ignore**). It does not represent a contiguous space. In the same way that read-only filesystems, those with quotas, or reserved space report the full theoretical space available.

Due to path preservation, branch tagging, read-only status, and **minfreespace** settings it is perfectly valid that `ENOSPC` / "out of space" / "no space left on device" be returned. It is doing what was asked of it: filtering possible branches due to those settings. Only one error can be returned and if one of the reasons for filtering a branch was **minfreespace** then it will be returned as such. **moveonenospc** is only relevant to writing a file which is too large for the drive its currently on.

It is also possible that the filesystem selected has run out of inodes. Use `df -i` to list the total and available inodes per filesystem.

If you don't care about path preservation then simply change the `create` policy to one which isn't. `mfs` is probably what most are looking for. The reason its not default is because it was originally set to `epmfs` and changing it now would change people's setup. Such a setting change will likely occur in mergerfs 3.


#### Can mergerfs mounts be exported over NFS?

Yes. Due to current usage of libfuse by mergerfs and how NFS interacts with it it is necessary to add `noforget` to mergerfs options to keep from getting "stale file handle" errors.

Some clients (Kodi) have issues in which the contents of the NFS mount will not be presented but users have found that enabling the `use_ino` option often fixes that problem.


#### Can mergerfs mounts be exported over Samba / SMB?

Yes. While some users have reported problems it appears to always be related to how Samba is setup in relation to permissions.


#### How are inodes calculated?

mergerfs-inode = (original-inode | (device-id << 32))

While `ino_t` is 64 bits only a few filesystems use more than 32. Similarly, while `dev_t` is also 64 bits it was traditionally 16 bits. Bitwise or'ing them together should work most of the time. While totally unique inodes are preferred the overhead which would be needed does not seem to outweighted by the benefits.

While atypical, yes, inodes can be reused and not refer to the same file. The internal id used to reference a file in FUSE is different from the inode value presented. The former is the `nodeid` and is actually a tuple of (nodeid,generation). That tuple is not user facing. The inode is merely metadata passed through the kernel and found using the `stat` family of calls or `readdir`.

From FUSE docs regarding `use_ino`:

```
Honor the st_ino field in the functions getattr() and
fill_dir(). This value is used to fill in the st_ino field
in the stat(2), lstat(2), fstat(2) functions and the d_ino
field in the readdir(2) function. The filesystem does not
have to guarantee uniqueness, however some applications
rely on this value being unique for the whole filesystem.
Note that this does *not* affect the inode that libfuse
and the kernel use internally (also called the "nodeid").
```


#### I notice massive slowdowns of writes over NFS

Due to how NFS works and interacts with FUSE when not using `cache.files=off` or `direct_io` its possible that a getxattr for `security.capability` will be issued prior to any write. This will usually result in a massive slowdown for writes. Using `cache.files=off` or `direct_io` will keep this from happening (and generally good to enable unless you need the features it disables) but the `security_capability` option can also help by short circuiting the call and returning `ENOATTR`.

You could also set `xattr` to `noattr` or `nosys` to short circuit or stop all xattr requests.


#### What are these .fuse_hidden files?

NOTE: mergerfs >= 2.26.0 will not have these temporary files. See the notes on `unlink`.

When not using **hard_remove** libfuse will create .fuse_hiddenXXXXXXXX files when an opened file is unlinked. This is to simplify "use after unlink" usecases. There is a possibility these files end up being picked up by software scanning directories and not ignoring hidden files. This is rarely a problem but a solution is in the works.

The files are cleaned up once the file is finally closed. Only if mergerfs crashes or is killed would they be left around. They are safe to remove as they are already unlinked files.


#### It's mentioned that there are some security issues with mhddfs. What are they? How does mergerfs address them?

[mhddfs](https://github.com/trapexit/mhddfs) manages running as **root** by calling [getuid()](https://github.com/trapexit/mhddfs/blob/cae96e6251dd91e2bdc24800b4a18a74044f6672/src/main.c#L319) and if it returns **0** then it will [chown](http://linux.die.net/man/1/chown) the file. Not only is that a race condition but it doesn't handle other situations. Rather than attempting to simulate POSIX ACL behavior the proper way to manage this is to use [seteuid](http://linux.die.net/man/2/seteuid) and [setegid](http://linux.die.net/man/2/setegid), in effect becoming the user making the original call, and perform the action as them. This is what mergerfs does and why mergerfs should always run as root.

In Linux setreuid syscalls apply only to the thread. GLIBC hides this away by using realtime signals to inform all threads to change credentials. Taking after **Samba**, mergerfs uses **syscall(SYS_setreuid,...)** to set the callers credentials for that thread only. Jumping back to **root** as necessary should escalated privileges be needed (for instance: to clone paths between drives).

For non-Linux systems mergerfs uses a read-write lock and changes credentials only when necessary. If multiple threads are to be user X then only the first one will need to change the processes credentials. So long as the other threads need to be user X they will take a readlock allowing multiple threads to share the credentials. Once a request comes in to run as user Y that thread will attempt a write lock and change to Y's credentials when it can. If the ability to give writers priority is supported then that flag will be used so threads trying to change credentials don't starve. This isn't the best solution but should work reasonably well assuming there are few users.


# PERFORMANCE TWEAKING

NOTE: be sure to read about these features before changing them

* enable (or disable) `splice_move`, `splice_read`, and `splice_write`
* increase cache timeouts `cache.attr`, `cache.entry`, `cache.negative_entry`
* enable (or disable) page caching (`cache.files`)
* enable `cache.open`
* enable `cache.statfs`
* enable `cache.symlinks`
* enable `cache.readdir`
* change the number of worker threads
* disable `security_capability` and/or `xattr`
* disable `posix_acl`
* disable `async_read`
* test theoretical performance using `nullrw` or mounting a ram disk
* use `symlinkify` if your data is largely static
* use tiered cache drives
* use lvm and lvm cache to place a SSD in front of your HDDs (howto coming)


# SUPPORT

Filesystems are very complex and difficult to debug. mergerfs, while being just a proxy of sorts, is also very difficult to debug given the large number of possible settings it can have itself and the massive number of environments it can run in. When reporting on a suspected issue **please, please** include as much of the below information as possible otherwise it will be difficult or impossible to diagnose. Also please make sure to read all of the above documentation as it includes nearly every known system or user issue previously encountered.


#### Information to include in bug reports

* Version of mergerfs: `mergerfs -V`
* mergerfs settings: from `/etc/fstab` or command line execution
* Version of Linux: `uname -a`
* Versions of any additional software being used
* List of drives, their filesystems, and sizes (before and after issue): `df -h`
* A `strace` of the app having problems:
  * `strace -f -o /tmp/app.strace.txt <cmd>`
* A `strace` of mergerfs while the program is trying to do whatever it's failing to do:
  * `strace -f -p <mergerfsPID> -o /tmp/mergerfs.strace.txt`
* **Precise** directions on replicating the issue. Do not leave **anything** out.
* Try to recreate the problem in the simplist way using standard programs.


#### Contact / Issue submission

* github.com: https://github.com/trapexit/mergerfs/issues
* email: trapexit@spawn.link
* twitter: https://twitter.com/_trapexit
* reddit: https://www.reddit.com/user/trapexit
* discord: https://discord.gg/MpAr69V


#### Support development

This software is free to use and released under a very liberal license. That said if you like this software and would like to support its development donations are welcome.

* PayPal: https://paypal.me/trapexit
* Patreon: https://www.patreon.com/trapexit
* SubscribeStar: https://www.subscribestar.com/trapexit
* Bitcoin (BTC): 12CdMhEPQVmjz3SSynkAEuD5q9JmhTDCZA
* Bitcoin Cash (BCH): 1AjPqZZhu7GVEs6JFPjHmtsvmDL4euzMzp
* Ethereum (ETH): 0x09A166B11fCC127324C7fc5f1B572255b3046E94
* Litecoin (LTC): LXAsq6yc6zYU3EbcqyWtHBrH1Ypx4GjUjm


# LINKS

* https://spawn.link
* https://github.com/trapexit/mergerfs
* https://github.com/trapexit/mergerfs-tools
* https://github.com/trapexit/scorch
* https://github.com/trapexit/bbf
* https://github.com/trapexit/backup-and-recovery-howtos
