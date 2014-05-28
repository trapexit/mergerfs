mergerfs
========

another FUSE union filesystem

mergerfs is similar to mhddfs, unionfs, and aufs. Like mhddfs in that it too uses FUSE. Like aufs in that it provides multiple policies for how to handle behavior.

Why create mergerfs when those exist? mhddfs isn't really maintained or flexible. There are also issues with running as root. aufs is more flexible but contains some hard to debug inconsistencies in behavior. Neither support file attributes ([chattr](http://linux.die.net/man/1/chattr)).

Policies
========

Filesystem calls are broken up into 4 functional categories: search, action, create, and none. These categories can be assigned a policy which dictates how [mergerfs](http://github.com/trapexit/mergerfs) behaves while when action on the filesystem. Any policy can be assigned to a category though some aren't terribly practical. For instance: rand (Random) may be useful for **create** but could lead to very odd behavior if used for **search** or **action**. Since the input for any policy is the source mounts and fusepath and the output a vector of targets the choice was made to simplify the implementation and allow a policies usage in any category. **NOTE:** In any policy which can return more than one location (currently only **all**) the first value will be used in **search** and **create** policies since they can only ever act on 1 filepath.

#### Functional classifications ####
| Class | FUSE calls |
|-------|------------|
| search | access, getattr, getxattr, listxattr, open, readlink  |
| action | chmod, link, removexattr, rmdir, setxattr, truncate, unlink, utimens |
| create | create, mkdir, mknod |
| none   | fallocate, fgetattr, fsync, ftruncate, ioctl, read, readdir, rename, statfs, symlink, write, release |

#### Policy descriptions ####
| Policy | Description |
|--------------|-------------|
| ff (first found) | Given the order the paths were provided at mount time act on the first one found (regardless if stat would return EACCES). |
| ffwp (first found w/ permissions) | Given the order the paths were provided at mount time act on the first one found which you have access (stat does not error with EACCES). |
| newest (newest file) | If multiple files exist return the one with the most recent mtime. |
| all (all files found) | Attempt to apply the call to each file found. If any sub call succeeds the entire operation succeeds and other errors ignored. If all fail then the last error is reported. |
| mfs (most free space) | Assuming the path is found to exist (ENOENT would not be returned) use the drive with the most free space available. |
| epmfs (existing path, most free space) | If the path exists in multiple locations use the one with the most free space. Otherwise fall back to mfs. |
| rand (random) | Pick a destination at random. Again the dirname of the full path must exist somewhere. |

#### statvfs ####

Since we aren't trying to stripe data across drives or move them should ENOSPC be return on a write the free space of the mountpoint is just that of the source mount with the most free space at the moment.

**NOTE:** create is really a search for existence and then create. The 'search' policy applies to the first part. If the [dirname](http://linux.die.net/man/3/dirname) of the full path is not found to exist [ENOENT](http://linux.die.net/man/3/errno) is returned.

Usage
=====

```
$ mergerfs -o create=epmfs,search=ff,action=ff <mountpoint> <dir0>:<dir1>:<dir2>
```

| Option | Default |
|--------|--------|
| search | ff |
| action | ff |
| create | epmfs |

Building
========

* Need to install FUSE development libraries (libfuse-dev).
* Optionally need libattr1 (libattr1-dev).


```
[trapexit:~/dev/mergerfs] $ make help
usage: make
make WITHOUT_XATTR=1 - to build program without xattrs functionality
```

Runtime Settings
================

#### /.mergerfs pseudo file ####
```
<mountpoint>/.mergerfs
```

There is a pseudo file available at the mountpoint which allows for the runtime modification of policies. The file will not show up in readdirs but can be stat'ed, read, and writen. Most other calls will fail with EPERM, EINVAL, or whatever may be appropriate for that call. Anything not understood while writing will result in EINVAL otherwise the number of bytes written will be returned.

Reading the file will result in a newline delimited list of current settings as followed:

```
[trapexit:/tmp/mount] $ cat .mergerfs
action=ff
create=epmfs
search=ff
```

Writing to the file is buffered and waits till a newline to process. Meaning echo works well.

```
[trapexit:/tmp/mount] $ echo "search=newest" >> .mergerfs
[trapexit:/tmp/mount] $ cat .mergerfs
action=ff
create=epmfs
search=newest
```

*NOTE:* offset is not supported and ignored in both read and write. There is also a safety check which limits buffered + incoming length to a max of 1024 bytes.

#### xattrs ####

If xattrs has been enabled you can also use [{list,get,set}xattrs](http://linux.die.net/man/2/listxattr) on the pseudo file **.mergerfs** to modify the policies. The keys are **mergerfs.action**, **mergerfs.create**, and **mergerfs.search**.

```
[trapexit:/tmp/mount] $ attr -l .mergerfs
Attribute "mergerfs.action" has a 2 byte value for .mergerfs
Attribute "mergerfs.create" has a 5 byte value for .mergerfs
Attribute "mergerfs.search" has a 2 byte value for .mergerfs

[trapexit:/tmp/mount] $ attr -g mergerfs.action .mergerfs
Attribute "mergerfs.action" had a 2 byte value for .mergerfs:
ff

[trapexit:/tmp/mount] 1 $ attr -s mergerfs.action -V ffwp .mergerfs
Attribute "mergerfs.action" set to a 4 byte value for .mergerfs:
ffwp
```
