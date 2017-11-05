% mergerfs(1) mergerfs user manual
% Antonio SJ Musumeci <trapexit@spawn.link>
% 2017-05-26

# NAME

mergerfs - a featureful union filesystem

# SYNOPSIS

mergerfs -o&lt;options&gt; &lt;srcmounts&gt; &lt;mountpoint&gt;

# DESCRIPTION

**mergerfs** is a union filesystem geared towards simplifying storage and management of files across numerous commodity storage devices. It is similar to **mhddfs**, **unionfs**, and **aufs**.

# FEATURES

* Runs in userspace (FUSE)
* Configurable behaviors
* Support for extended attributes (xattrs)
* Support for file attributes (chattr)
* Runtime configurable (via xattrs)
* Safe to run as root
* Opportunistic credential caching
* Works with heterogeneous filesystem types
* Handling of writes to full drives (transparently move file to drive with capacity)
* Handles pool of readonly and read/write drives
* Turn read-only files into symlinks to increase read performance

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

mergerfs does **not** support the copy-on-write (CoW) behavior found in **aufs** and **overlayfs**. You can **not** mount a read-only filesystem and write to it. However, mergerfs will ignore read-only drives when creating new files so you can mix rw and ro drives.

# OPTIONS

### mount options

* **defaults**: a shortcut for FUSE's **atomic_o_trunc**, **auto_cache**, **big_writes**, **default_permissions**, **splice_move**, **splice_read**, and **splice_write**. These options seem to provide the best performance.
* **direct_io**: causes FUSE to bypass caching which can increase write speeds at the detriment of reads. Note that not enabling `direct_io` will cause double caching of files and therefore less memory for caching generally. However, `mmap` does not work when `direct_io` is enabled.
* **minfreespace=value**: the minimum space value used for creation policies. Understands 'K', 'M', and 'G' to represent kilobyte, megabyte, and gigabyte respectively. (default: 4G)
* **moveonenospc=true|false**: when enabled (set to **true**) if a **write** fails with **ENOSPC** or **EDQUOT** a scan of all drives will be done looking for the drive with most free space which is at least the size of the file plus the amount which failed to write. An attempt to move the file to that drive will occur (keeping all metadata possible) and if successful the original is unlinked and the write retried. (default: false)
* **use_ino**: causes mergerfs to supply file/directory inodes rather than libfuse. While not a default it is generally recommended it be enabled so that hard linked files share the same inode value.
* **dropcacheonclose=true|false**: when a file is requested to be closed call `posix_fadvise` on it first to instruct the kernel that we no longer need the data and it can drop its cache. Recommended when **direct_io** is not enabled to limit double caching. (default: false)
* **symlinkify=true|false**: when enabled (set to **true**) and a file is not writable and its mtime or ctime is older than **symlinkify_timeout** files will be reported as symlinks to the original files. Please read more below before using. (default: false)
* **symlinkify_timeout=value**: time to wait, in seconds, to activate the **symlinkify** behavior. (default: 3600)
* **nullrw=true|false**: turns reads and writes into no-ops. The request will succeed but do nothing. Useful for benchmarking mergerfs. (default: false)
* **ignorepponrename=true|false**: ignore path preserving on rename. Typically rename and link act differently depending on the policy of `create` (read below). Enabling this will cause rename and link to always use the non-path preserving behavior. This means files, when renamed or linked, will stay on the same drive. (default: false)
* **threads=num**: number of threads to use in multithreaded mode. When set to zero (the default) it will attempt to discover and use the number of logical cores. If the lookup fails it will fall back to using 4. If the thread count is set negative it will look up the number of cores then divide by the absolute value. ie. threads=-2 on an 8 core machine will result in 8 / 2 = 4 threads. There will always be at least 1 thread. NOTE: higher number of threads increases parallelism but usually decreases throughput. (default: number of cores)
* **fsname=name**: sets the name of the filesystem as seen in **mount**, **df**, etc. Defaults to a list of the source paths concatenated together with the longest common prefix removed.
* **func.&lt;func&gt;=&lt;policy&gt;**: sets the specific FUSE function's policy. See below for the list of value types. Example: **func.getattr=newest**
* **category.&lt;category&gt;=&lt;policy&gt;**: Sets policy of all FUSE functions in the provided category. Example: **category.create=mfs**

**NOTE:** Options are evaluated in the order listed so if the options are **func.rmdir=rand,category.action=ff** the **action** category setting will override the **rmdir** setting.

### srcmounts

The srcmounts (source mounts) argument is a colon (':') delimited list of paths to be included in the pool. It does not matter if the paths are on the same or different drives nor does it matter the filesystem. Used and available space will not be duplicated for paths on the same device and any features which aren't supported by the underlying filesystem (such as file attributes or extended attributes) will return the appropriate errors.

To make it easier to include multiple source mounts mergerfs supports [globbing](http://linux.die.net/man/7/glob). **The globbing tokens MUST be escaped when using via the shell else the shell itself will expand it.**

```
$ mergerfs -o defaults,allow_other,use_ino /mnt/disk\*:/mnt/cdrom /media/drives
```

The above line will use all mount points in /mnt prefixed with **disk** and the **cdrom**.

To have the pool mounted at boot or otherwise accessable from related tools use **/etc/fstab**.

```
# <file system>        <mount point>  <type>         <options>                     <dump>  <pass>
/mnt/disk*:/mnt/cdrom  /media/drives  fuse.mergerfs  defaults,allow_other,use_ino  0       0
```

**NOTE:** the globbing is done at mount or xattr update time (see below). If a new directory is added matching the glob after the fact it will not be automatically included.

**NOTE:** for mounting via **fstab** to work you must have **mount.fuse** installed. For Ubuntu/Debian it is included in the **fuse** package.

### symlinkify

Due to the levels of indirection introduced by mergerfs and the underlying technology FUSE there can be varying levels of performance degredation. This feature will turn non-directories which are not writable into symlinks to the original file found by the `readlink` policy after the mtime and ctime are older than the timeout.

**WARNING:** The current implementation has a known issue in which if the file is open and being used when the file is converted to a symlink then the application which has that file open will receive an error when using it. This is unlikely to occur in practice but is something to keep in mind.

**WARNING:** Some backup solutions, such as CrashPlan, do not backup the target of a symlink. If using this feature it will be necessary to point any backup software to the original drives or configure the software to follow symlinks if such an option is available. Alternatively create two mounts. One for backup and one for general consumption.

### nullrw

Due to how FUSE works there is an overhead to all requests made to a FUSE filesystem. Meaning that even a simple passthrough will have some slowdown. However, generally the overhead is minimal in comparison to the cost of the underlying I/O. By disabling the underlying I/O we can test the theoretical performance boundries.

By enabling `nullrw` mergerfs will work as it always does **except** that all reads and writes will be no-ops. A write will succeed (the size of the write will be returned as if it were successful) but mergerfs does nothing with the data it was given. Similarly a read will return the size requested but won't touch the buffer.

Example:
```
$ dd if=/dev/zero of=/path/to/mergerfs/mount/benchmark ibs=1M obs=512 count=1024
1024+0 records in
2097152+0 records out
1073741824 bytes (1.1 GB, 1.0 GiB) copied, 15.4067 s, 69.7 MB/s

$ dd if=/dev/zero of=/path/to/mergerfs/mount/benchmark ibs=1M obs=1M count=1024
1024+0 records in
1024+0 records out
1073741824 bytes (1.1 GB, 1.0 GiB) copied, 0.219585 s, 4.9 GB/s

$ dd if=/path/to/mergerfs/mount/benchmark of=/dev/null bs=512 count=102400
102400+0 records in
102400+0 records out
52428800 bytes (52 MB, 50 MiB) copied, 0.757991 s, 69.2 MB/s

$ dd if=/path/to/mergerfs/mount/benchmark of=/dev/null bs=1M count=1024
1024+0 records in
1024+0 records out
1073741824 bytes (1.1 GB, 1.0 GiB) copied, 0.18405 s, 5.8 GB/s
```

It's important to test with different `obs` (output block size) values since the relative overhead is greater with smaller values. As you can see above the size of a read or write can massively impact theoretical performance. If an application performs much worse through mergerfs it could very well be that it doesn't optimally size its read and write requests.

# FUNCTIONS / POLICIES / CATEGORIES

The POSIX filesystem API has a number of functions. **creat**, **stat**, **chown**, etc. In mergerfs these functions are grouped into 3 categories: **action**, **create**, and **search**. Functions and categories can be assigned a policy which dictates how **mergerfs** behaves. Any policy can be assigned to a function or category though some may not be very useful in practice. For instance: **rand** (random) may be useful for file creation (create) but could lead to very odd behavior if used for `chmod` (though only if there were more than one copy of the file).

Policies, when called to create, will ignore drives which are readonly. This allows for readonly and read/write drives to be mixed together. Note that the drive must be explicitly mounted with the **ro** mount option for this to work.

#### Function / Category classifications

| Category | FUSE Functions                                                                      |
|----------|-------------------------------------------------------------------------------------|
| action   | chmod, chown, link, removexattr, rename, rmdir, setxattr, truncate, unlink, utimens |
| create   | create, mkdir, mknod, symlink                                                       |
| search   | access, getattr, getxattr, ioctl, listxattr, open, readlink                         |
| N/A      | fallocate, fgetattr, fsync, ftruncate, ioctl, read, readdir, release, statfs, write |

Due to FUSE limitations **ioctl** behaves differently if its acting on a directory. It'll use the **getattr** policy to find and open the directory before issuing the **ioctl**. In other cases where something may be searched (to confirm a directory exists across all source mounts) **getattr** will also be used.

#### Path Preservation

Policies, as described below, are of two core types. `path preserving` and `non-path preserving`.

All policies which start with `ep` (**epff**, **eplfs**, **eplus**, **epmfs**, **eprand**) are `path preserving`. `ep` stands for `existing path`.

As the descriptions explain a path preserving policy will only consider drives where the relative path being accessed already exists.

When using non-path preserving policies where something is created paths will be copied to target drives as necessary.

#### Policy descriptions

| Policy           | Description                                                |
|------------------|------------------------------------------------------------|
| all | Search category: acts like **ff**. Action category: apply to all found. Create category: for **mkdir**, **mknod**, and **symlink** it will apply to all found. **create** works like **ff**. It will exclude readonly drives and those with free space less than **minfreespace**. |
| epall (existing path, all) | Search category: acts like **epff**. Action category: apply to all found. Create category: for **mkdir**, **mknod**, and **symlink** it will apply to all existing paths found. **create** works like **epff**. Excludes readonly drives and those with free space less than **minfreespace**. |
| epff (existing path, first found) | Given the order of the drives, as defined at mount time or configured at runtime, act on the first one found where the relative path already exists. For **create** category functions it will exclude readonly drives and those with free space less than **minfreespace** (unless there is no other option). Falls back to **ff**. |
| eplfs (existing path, least free space) | Of all the drives on which the relative path exists choose the drive with the least free space. For **create** category functions it will exclude readonly drives and those with free space less than **minfreespace**. Falls back to **lfs**. |
| eplus (existing path, least used space) | Of all the drives on which the relative path exists choose the drive with the least used space. For **create** category functions it will exclude readonly drives and those with free space less than **minfreespace**. Falls back to **lus**. |
| epmfs (existing path, most free space) | Of all the drives on which the relative path exists choose the drive with the most free space. For **create** category functions it will exclude readonly drives and those with free space less than **minfreespace**. Falls back to **mfs**. |
| eprand (existing path, random) | Calls **epall** and then randomizes. Otherwise behaves the same as **epall**. |
| erofs | Exclusively return **-1** with **errno** set to **EROFS** (Read-only filesystem). By setting **create** functions to this you can in effect turn the filesystem mostly readonly. |
| ff (first found) | Given the order of the drives, as defined at mount time or configured at runtime, act on the first one found. For **create** category functions it will exclude readonly drives and those with free space less than **minfreespace** (unless there is no other option). |
| lfs (least free space) | Pick the drive with the least available free space. For **create** category functions it will exclude readonly drives and those with free space less than **minfreespace**. Falls back to **mfs**. |
| lus (least used space) | Pick the drive with the least used space. For **create** category functions it will exclude readonly drives and those with free space less than **minfreespace**. Falls back to **mfs**. |
| mfs (most free space) | Pick the drive with the most available free space. For **create** category functions it will exclude readonly drives. Falls back to **ff**. |
| newest | Pick the file / directory with the largest mtime. For **create** category functions it will exclude readonly drives and those with free space less than **minfreespace** (unless there is no other option). |
| rand (random) | Calls **all** and then randomizes. |

#### Defaults ####

| Category | Policy |
|----------|--------|
| action   | all    |
| create   | epmfs  |
| search   | ff     |

#### rename & link ####

**NOTE:** If you're receiving errors from software when files are moved / renamed then you should consider changing the create policy to one which is **not** path preserving, enabling `ignorepponrename`, or contacting the author of the offending software and requesting that `EXDEV` be properly handled.

[rename](http://man7.org/linux/man-pages/man2/rename.2.html) is a tricky function in a merged system. Under normal situations rename only works within a single filesystem or device. If a rename can't be done atomically due to the source and destination paths existing on different mount points it will return **-1** with **errno = EXDEV** (cross device).

Originally mergerfs would return EXDEV whenever a rename was requested which was cross directory in any way. This made the code simple and was technically complient with POSIX requirements. However, many applications fail to handle EXDEV at all and treat it as a normal error or otherwise handle it poorly. Such apps include: gvfsd-fuse v1.20.3 and prior, Finder / CIFS/SMB client in Apple OSX 10.9+, NZBGet, Samba's recycling bin feature.

As a result a compromise was made in order to get most software to work while still obeying mergerfs' policies. Below is the rather complicated logic.

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

**link** uses the same basic strategy.

#### readdir ####

[readdir](http://linux.die.net/man/3/readdir) is different from all other filesystem functions. While it could have it's own set of policies to tweak its behavior at this time it provides a simple union of files and directories found. Remember that any action or information queried about these files and directories come from the respective function. For instance: an **ls** is a **readdir** and for each file/directory returned **getattr** is called. Meaning the policy of **getattr** is responsible for choosing the file/directory which is the source of the metadata you see in an **ls**.

#### statvfs ####

[statvfs](http://linux.die.net/man/2/statvfs) normalizes the source drives based on the fragment size and sums the number of adjusted blocks and inodes. This means you will see the combined space of all sources. Total, used, and free. The sources however are dedupped based on the drive so multiple sources on the same drive will not result in double counting it's space.

# BUILDING

**NOTE:** Prebuilt packages can be found at: https://github.com/trapexit/mergerfs/releases

First get the code from [github](http://github.com/trapexit/mergerfs).

```
$ git clone https://github.com/trapexit/mergerfs.git
$ # or
$ wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-<ver>.tar.gz
```

#### Debian / Ubuntu
```
$ sudo apt-get -y update
$ sudo apt-get -y install git make
$ cd mergerfs
$ make install-build-pkgs
$ # build-essential git g++ debhelper libattr1-dev python automake libtool lsb-release
$ make deb
$ sudo dpkg -i ../mergerfs_version_arch.deb
```

#### Fedora
```
$ su -
# dnf -y update
# dnf -y install git make
# cd mergerfs
# make install-build-pkgs
# # rpm-build libattr-devel gcc-c++ which python automake libtool gettext-devel
# make rpm
# rpm -i rpmbuild/RPMS/<arch>/mergerfs-<verion>.<arch>.rpm
```

#### Generically

Have git, g++, make, python, libattr1, automake, libtool installed.

```
$ cd mergerfs
$ make
$ sudo make install
```

# RUNTIME

#### .mergerfs pseudo file ####
```
<mountpoint>/.mergerfs
```

There is a pseudo file available at the mount point which allows for the runtime modification of certain **mergerfs** options. The file will not show up in **readdir** but can be **stat**'ed and manipulated via [{list,get,set}xattrs](http://linux.die.net/man/2/listxattr) calls.

Even if xattrs are disabled for mergerfs the [{list,get,set}xattrs](http://linux.die.net/man/2/listxattr) calls against this pseudo file will still work.

Any changes made at runtime are **not** persisted. If you wish for values to persist they must be included as options wherever you configure the mounting of mergerfs (fstab).

##### Keys #####

Use `xattr -l /mount/point/.mergerfs` to see all supported keys. Some are informational and therefore readonly.

###### user.mergerfs.srcmounts ######

Used to query or modify the list of source mounts. When modifying there are several shortcuts to easy manipulation of the list.

| Value        | Description |
|--------------|-------------|
| [list]       | set         |
| +<[list]     | prepend     |
| +>[list]     | append      |
| -[list]      | remove all values provided |
| -<           | remove first in list |
| ->           | remove last in list |

###### minfreespace ######

Input: interger with an optional multiplier suffix. **K**, **M**, or **G**.

Output: value in bytes

###### moveonenospc ######

Input: **true** and **false**

Ouput: **true** or **false**

###### categories / funcs ######

Input: short policy string as described elsewhere in this document

Output: the policy string except for categories where its funcs have multiple types. In that case it will be a comma separated list

##### Example #####

```
[trapexit:/tmp/mount] $ xattr -l .mergerfs
user.mergerfs.srcmounts: /tmp/a:/tmp/b
user.mergerfs.minfreespace: 4294967295
user.mergerfs.moveonenospc: false
...

[trapexit:/tmp/mount] $ xattr -p user.mergerfs.category.search .mergerfs
ff

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.category.search newest .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.category.search .mergerfs
newest

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.srcmounts +/tmp/c .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.srcmounts .mergerfs
/tmp/a:/tmp/b:/tmp/c

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.srcmounts =/tmp/c .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.srcmounts .mergerfs
/tmp/c

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.srcmounts '+</tmp/a:/tmp/b' .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.srcmounts .mergerfs
/tmp/a:/tmp/b:/tmp/c
```

#### file / directory xattrs ####

While they won't show up when using [listxattr](http://linux.die.net/man/2/listxattr) **mergerfs** offers a number of special xattrs to query information about the files served. To access the values you will need to issue a [getxattr](http://linux.die.net/man/2/getxattr) for one of the following:

* **user.mergerfs.basepath:** the base mount point for the file given the current getattr policy
* **user.mergerfs.relpath:** the relative path of the file from the perspective of the mount point
* **user.mergerfs.fullpath:** the full path of the original file given the getattr policy
* **user.mergerfs.allpaths:** a NUL ('\0') separated list of full paths to all files found

```
[trapexit:/tmp/mount] $ ls
A B C
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.fullpath A
/mnt/a/full/path/to/A
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.basepath A
/mnt/a
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.relpath A
/full/path/to/A
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.allpaths A | tr '\0' '\n'
/mnt/a/full/path/to/A
/mnt/b/full/path/to/A
```

# TOOLING

* https://github.com/trapexit/mergerfs-tools
  * mergerfs.ctl: A tool to make it easier to query and configure mergerfs at runtime
  * mergerfs.fsck: Provides permissions and ownership auditing and the ability to fix them
  * mergerfs.dedup: Will help identify and optionally remove duplicate files
  * mergerfs.balance: Rebalance files across drives by moving them from the most filled to the least filled
  * mergerfs.mktrash: Creates FreeDesktop.org Trash specification compatible directories on a mergerfs mount
* https://github.com/trapexit/scorch
  * scorch: A tool to help discover silent corruption of files
* https://github.com/trapexit/bbf
  * bbf (bad block finder): a tool to scan for and 'fix' hard drive bad blocks and find the files using those blocks

# CACHING

MergerFS does not natively support any sort of caching. Most users have no use for such a feature and it would greatly complicate the code. However, there are a few situations where a cache drive could help with a typical mergerfs setup.

1. Fast network, slow drives, many readers: You've a 10+Gbps network with many readers and your regular drives can't keep up.
2. Fast network, slow drives, small'ish bursty writes: You have a 10+Gbps network and wish to transfer amounts of data less than your cache drive but wish to do so quickly.

The below will mostly address usecase #2. It will also work for #1 assuming the data is regularly accessed and was placed into the system via this method. Otherwise a similar script may need to be written to populate the cache from the backing pool.

1. Create 2 mergerfs pools. One which includes just the backing drives and one which has both the cache drives (SSD,NVME,etc.) and backing drives.
2. The 'cache' pool should have the cache drives listed first.
3. The best policies to use for the 'cache' pool would probably be `ff`, `epff`, `lfs`, or `eplfs`. The latter two under the assumption that the cache drive(s) are far smaller than the backing drives. If using path preserving policies remember that you'll need to manually create the core directories of those paths you wish to be cached. (Be sure the permissions are in sync. Use `mergerfs.fsck` to check / correct them.)
4. Enable `moveonenospc` and set `minfreespace` appropriately.
5. Set your programs to use the cache pool.
6. Save one of the below scripts.
7. Use `crontab` (as root) to schedule the command at whatever frequency is appropriate for your workflow.

### Time based expiring

Move files from cache to backing pool based only on the last time the file was accessed.

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
  rsync --files-from=- -aq --remove-source-files "${CACHE}/" "${BACKING}/"
```

### Percentage full expiring

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
    rsync -aq --remove-source-files "${CACHE}/./${FILE}" "${BACKING}/"
done
```

# TIPS / NOTES

* The recommended options are **defaults,allow_other,direct_io,use_ino**. (**use_ino** will only work when used with mergerfs 2.18.0 and above.)
* Run mergerfs as `root` unless you're merging paths which are owned by the same user otherwise strange permission issues may arise.
* https://github.com/trapexit/backup-and-recovery-howtos : A set of guides / howtos on creating a data storage system, backing it up, maintaining it, and recovering from failure.
* If you don't see some directories and files you expect in a merged point or policies seem to skip drives be sure the user has permission to all the underlying directories. Use `mergerfs.fsck` to audit the drive for out of sync permissions.
* Do *not* use `direct_io` if you expect applications (such as rtorrent) to [mmap](http://linux.die.net/man/2/mmap) files. It is not currently supported in FUSE w/ `direct_io` enabled.
* Since POSIX gives you only error or success on calls its difficult to determine the proper behavior when applying the behavior to multiple targets. **mergerfs** will return an error only if all attempts of an action fail. Any success will lead to a success returned. This means however that some odd situations may arise.
* [Kodi](http://kodi.tv), [Plex](http://plex.tv), [Subsonic](http://subsonic.org), etc. can use directory [mtime](http://linux.die.net/man/2/stat) to more efficiently determine whether to scan for new content rather than simply performing a full scan. If using the default **getattr** policy of **ff** its possible **Kodi** will miss an update on account of it returning the first directory found's **stat** info and its a later directory on another mount which had the **mtime** recently updated. To fix this you will want to set **func.getattr=newest**. Remember though that this is just **stat**. If the file is later **open**'ed or **unlink**'ed and the policy is different for those then a completely different file or directory could be acted on.
* Some policies mixed with some functions may result in strange behaviors. Not that some of these behaviors and race conditions couldn't happen outside **mergerfs** but that they are far more likely to occur on account of attempt to merge together multiple sources of data which could be out of sync due to the different policies.
* For consistency its generally best to set **category** wide policies rather than individual **func**'s. This will help limit the confusion of tools such as [rsync](http://linux.die.net/man/1/rsync). However, the flexibility is there if needed.

# KNOWN ISSUES / BUGS

#### directory mtime is not being updated

Remember that the default policy for `getattr` is `ff`. The information for the first directory found will be returned. If it wasn't the directory which had been updated then it will appear outdated.

The reason this is the default is because any other policy would be far more expensive and for many applications it is unnecessary. To always return the directory with the most recent mtime or a faked value based on all found would require a scan of all drives. That alone is far more expensive than `ff` but would also possibly spin up sleeping drives.

If you always want the directory information from the one with the most recent mtime then use the `newest` policy for `getattr`.

#### cached memory appears greater than it should be

Use the `direct_io` option as described above. Due to what mergerfs is doing there ends up being two caches of a file under normal usage. One from the underlying filesystem and one from mergerfs. Enabling `direct_io` removes the mergerfs cache. This saves on memory but means the kernel needs to communicate with mergerfs more often and can therefore result in slower speeds.

Since enabling `direct_io` disables `mmap` this is not an ideal situation however write speeds should be increased.

If `direct_io` is disabled it is probably a good idea to enable `dropcacheonclose` to minimize double caching.

#### NFS clients don't work

Some NFS clients appear to fail when a mergerfs mount is exported. Kodi in particular seems to have issues.

Try enabling the `use_ino` option. Some have reported that it fixes the issue.

#### rtorrent fails with ENODEV (No such device)

Be sure to turn off `direct_io`. rtorrent and some other applications use [mmap](http://linux.die.net/man/2/mmap) to read and write to files and offer no failback to traditional methods. FUSE does not currently support mmap while using `direct_io`. There will be a performance penalty on writes with `direct_io` off as well as the problem of double caching but it's the only way to get such applications to work. If the performance loss is too high for other apps you can mount mergerfs twice. Once with `direct_io` enabled and one without it.

#### Plex doesn't work with mergerfs

It does. If you're trying to put Plex's config / metadata on mergerfs you have to leave `direct_io` off because Plex is using sqlite which apparently needs mmap. mmap doesn't work with `direct_io`.

If the issue is that scanning doesn't seem to pick up media then be sure to set `func.getattr=newest` as mentioned above.

#### mmap performance is really bad

There [is a bug](https://lkml.org/lkml/2016/3/16/260) in caching which affects overall performance of mmap through FUSE in Linux 4.x kernels. It is fixed in [4.4.10 and 4.5.4](https://lkml.org/lkml/2016/5/11/59).

#### When a program tries to move or rename a file it fails

Please read the section above regarding [rename & link](#rename--link).

The problem is that many applications do not properly handle `EXDEV` errors which `rename` and `link` may return even though they are perfectly valid situations which do not indicate actual drive or OS errors. The error will only be returned by mergerfs if using a path preserving policy as described in the policy section above. If you do not care about path preservation simply change the mergerfs policy to the non-path preserving version. For example: `-o category.create=mfs`

Ideally the offending software would be fixed and it is recommended that if you run into this problem you contact the software's author and request proper handling of `EXDEV` errors.

#### Samba: Moving files / directories fails

Workaround: Copy the file/directory and then remove the original rather than move.

This isn't an issue with Samba but some SMB clients. GVFS-fuse v1.20.3 and prior (found in Ubuntu 14.04 among others) failed to handle certain error codes correctly. Particularly **STATUS_NOT_SAME_DEVICE** which comes from the **EXDEV** which is returned by **rename** when the call is crossing mount points. When a program gets an **EXDEV** it needs to explicitly take an alternate action to accomplish it's goal. In the case of **mv** or similar it tries **rename** and on **EXDEV** falls back to a manual copying of data between the two locations and unlinking the source. In these older versions of GVFS-fuse if it received **EXDEV** it would translate that into **EIO**. This would cause **mv** or most any application attempting to move files around on that SMB share to fail with a IO error.

[GVFS-fuse v1.22.0](https://bugzilla.gnome.org/show_bug.cgi?id=734568) and above fixed this issue but a large number of systems use the older release. On Ubuntu the version can be checked by issuing `apt-cache showpkg gvfs-fuse`. Most distros released in 2015 seem to have the updated release and will work fine but older systems may not. Upgrading gvfs-fuse or the distro in general will address the problem.

In Apple's MacOSX 10.9 they replaced Samba (client and server) with their own product. It appears their new client does not handle **EXDEV** either and responds similar to older release of gvfs on Linux.

#### Trashing files occasionally fails

This is the same issue as with Samba. `rename` returns `EXDEV` (in our case that will really only happen with path preserving policies like `epmfs`) and the software doesn't handle the situtation well. This is unfortunately a common failure of software which moves files around. The standard indicates that an implementation `MAY` choose to support non-user home directory trashing of files (which is a `MUST`). The implementation `MAY` also support "top directory trashes" which many probably do.

To create a `$topdir/.Trash` directory as defined in the standard use the [mergerfs-tools](https://github.com/trapexit/mergerfs-tools) tool `mergerfs.mktrash`.

#### Supplemental user groups

Due to the overhead of [getgroups/setgroups](http://linux.die.net/man/2/setgroups) mergerfs utilizes a cache. This cache is opportunistic and per thread. Each thread will query the supplemental groups for a user when that particular thread needs to change credentials and will keep that data for the lifetime of the thread. This means that if a user is added to a group it may not be picked up without the restart of mergerfs. However, since the high level FUSE API's (at least the standard version) thread pool dynamically grows and shrinks it's possible that over time a thread will be killed and later a new thread with no cache will start and query the new data.

The gid cache uses fixed storage to simplify the design and be compatible with older systems which may not have C++11 compilers. There is enough storage for 256 users' supplemental groups. Each user is allowed upto 32 supplemental groups. Linux >= 2.6.3 allows upto 65535 groups per user but most other *nixs allow far less. NFS allowing only 16. The system does handle overflow gracefully. If the user has more than 32 supplemental groups only the first 32 will be used. If more than 256 users are using the system when an uncached user is found it will evict an existing user's cache at random. So long as there aren't more than 256 active users this should be fine. If either value is too low for your needs you will have to modify `gidcache.hpp` to increase the values. Note that doing so will increase the memory needed by each thread.

#### mergerfs or libfuse crashing

**NOTE:** as of mergerfs 2.22.0 it includes the most recent version of libfuse so any crash should be reported. For older releases continue reading...

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

There is a bug in the kernel. A work around appears to be turning off `splice`. Add `no_splice_write,no_splice_move,no_splice_read` to mergerfs' options. Should be placed after `defaults` if it is used since it will turn them on. This however is not guaranteed to work.

# FAQ

#### Why use mergerfs over mhddfs?

mhddfs is no longer maintained and has some known stability and security issues (see below). MergerFS provides a superset of mhddfs' features and should offer the same or maybe better performance.

If you wish to get similar behavior to mhddfs from mergerfs then set `category.create=ff`.

#### Why use mergerfs over aufs?

While aufs can offer better peak performance mergerfs provides more configurability and is generally easier to use. mergerfs however does not offer the overlay / copy-on-write (COW) features which aufs and overlayfs have.

#### Why use mergerfs over LVM/ZFS/BTRFS/RAID0 drive concatenation / striping?

With simple JBOD / drive concatenation / stripping / RAID0 a single drive failure will result in full pool failure. mergerfs performs a similar behavior without the possibility of catastrophic failure and difficulties in recovery. Drives may fail however all other data will continue to be accessable.

When combined with something like [SnapRaid](http://www.snapraid.it) and/or an offsite backup solution you can have the flexibilty of JBOD without the single point of failure.

#### Why use mergerfs over ZFS?

MergerFS is not intended to be a replacement for ZFS. MergerFS is intended to provide flexible pooling of arbitrary drives (local or remote), of arbitrary sizes, and arbitrary filesystems. For `write once, read many` usecases such as bulk media storage. Where data integrity and backup is managed in other ways. In that situation ZFS can introduce major maintance and cost burdens as described [here](http://louwrentius.com/the-hidden-cost-of-using-zfs-for-your-home-nas.html).

#### Can drives be written to directly? Outside of mergerfs while pooled?

Yes. It will be represented immediately in the pool as the policies perscribe.

#### Why do I get an "out of space" error even though the system says there's lots of space left?

First make sure you've read the sections above about policies, path preserving, and the **moveonenospc** option.

Remember that mergerfs is simply presenting a logical merging of the contents of the pooled drives. The reported free space is the aggregate space available **not** the contiguous space available. MergerFS does not split files across drives. If the writing of a file fills a drive and **moveonenospc** is disabled it will return an ENOSPC error.

If **moveonenospc** is enabled but there exists no drives with enough space for the file and the data to be written (or the drive happened to fill up as the file was being moved) it will error indicating there isn't enough space.

It is also possible that the filesystem selected has run out of inodes. Use `df -i` to list the total and available inodes per filesystem. In the future it might be worth considering the number of inodes available when making placement decisions in order to minimize this situation.

#### Can mergerfs mounts be exported over NFS?

Yes. Some clients (Kodi) have issues in which the contents of the NFS mount will not be presented but users have found that enabling the `use_ino` option often fixes that problem.

#### Can mergerfs mounts be exported over Samba / SMB?

Yes.

#### How are inodes calculated?

mergerfs-inode = (original-inode | (device-id << 32))

While `ino_t` is 64 bits only a few filesystems use more than 32. Similarly, while `dev_t` is also 64 bits it was traditionally 16 bits. Bitwise or'ing them together should work most of the time. While totally unique inodes are preferred the overhead which would be needed does not seem to outweighted by the benefits.

#### It's mentioned that there are some security issues with mhddfs. What are they? How does mergerfs address them?

[mhddfs](https://github.com/trapexit/mhddfs) manages running as **root** by calling [getuid()](https://github.com/trapexit/mhddfs/blob/cae96e6251dd91e2bdc24800b4a18a74044f6672/src/main.c#L319) and if it returns **0** then it will [chown](http://linux.die.net/man/1/chown) the file. Not only is that a race condition but it doesn't handle many other situations. Rather than attempting to simulate POSIX ACL behavior the proper way to manage this is to use [seteuid](http://linux.die.net/man/2/seteuid) and [setegid](http://linux.die.net/man/2/setegid), in effect becoming the user making the original call, and perform the action as them. This is what mergerfs does.

In Linux setreuid syscalls apply only to the thread. GLIBC hides this away by using realtime signals to inform all threads to change credentials. Taking after **Samba**, mergerfs uses **syscall(SYS_setreuid,...)** to set the callers credentials for that thread only. Jumping back to **root** as necessary should escalated privileges be needed (for instance: to clone paths between drives).

For non-Linux systems mergerfs uses a read-write lock and changes credentials only when necessary. If multiple threads are to be user X then only the first one will need to change the processes credentials. So long as the other threads need to be user X they will take a readlock allowing multiple threads to share the credentials. Once a request comes in to run as user Y that thread will attempt a write lock and change to Y's credentials when it can. If the ability to give writers priority is supported then that flag will be used so threads trying to change credentials don't starve. This isn't the best solution but should work reasonably well assuming there are few users.

# SUPPORT

Filesystems are very complex and difficult to debug. mergerfs, while being just a proxy of sorts, is also very difficult to debug given the large number of possible settings it can have itself and the massive number of environments it can run in. When reporting on a suspected issue **please, please** include as much of the below information as possible otherwise it will be difficult or impossible to diagnose. Also please make sure to read all of the above documentation as it includes nearly every common system or user issue.

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
* **Precise** directions on replicating the issue. Don't leave **anything** out.
* Try to recreate the problem in the simplist way using standard programs.

#### Issue submission / Contact
* github.com: https://github.com/trapexit/mergerfs/issues
* email: trapexit@spawn.link
* twitter: https://twitter.com/_trapexit

#### Support development
* BitCoin: 12CdMhEPQVmjz3SSynkAEuD5q9JmhTDCZA
* PayPal: trapexit@spawn.link
* Patreon: https://www.patreon.com/trapexit
* Gratipay: https://gratipay.com/~trapexit

# LINKS

* http://github.com/trapexit/mergerfs
* http://github.com/trapexit/mergerfs-tools
* http://github.com/trapexit/scorch
* http://github.com/trapexit/backup-and-recovery-howtos
