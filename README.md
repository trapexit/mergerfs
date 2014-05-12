mergerfs
========

another FUSE union filesystem

mergerfs is similar to mhddfs, unionfs, and aufs. Like mhddfs in that it too uses FUSE. Like aufs in that it provides multiple policies for how to handle behavior.

Why create mergerfs when those exist? mhddfs isn't really maintained or flexible. There are also issues with running as root. aufs is more flexible but contains some hard to debug inconsistencies in behavior. Neither support file attributes ([chattr](http://linux.die.net/man/1/chattr)).

Policies
========

Filesystem calls are broken up into 5 classes of policies: search, action, create, statfs, and none.

Below shows each policy class, the FUSE calls they impact, and the policy names.

####Policy classifications####
| Class | FUSE calls | Policies |
|-------|------------|----------|
| search | access, getattr, getxattr, listxattr, open, readlink  | First Found (ff), First Found w/ Permission (ffwp), Newest (newest) |
| action | chmod, link, removexattr, rmdir, setxattr, truncate, unlink, utimens | First Found (ff), First Found w/ Permission (ffwp), Newest (newest), All Found (all) |
| create | create, mkdir, mknod | Existing Path (ep), Most Free Space (mfs), Existing Path Most Free Space (epmfs), Random (rand) |
| statfs | statfs | Sum Used Max Free (sumf), Sum Used Sum Free (susf) |
| none   | fallocate, fsync, ftruncate, ioctl, read, readdir, rename, symlink, write, release | |

####Descriptions####
| Class/Policy | Description |
|--------------|-------------|
| search/ff | Given the order the paths were provided at mount time act on the first one found (regardless if stat would return EACCES). |
| search/ffwp | Given the order the paths were provided at mount time act on the first one found which you have access (stat does not error with EACCES). |
| search/newest | If multiple files exist return the one with the most recent mtime. |
| action/ff | (same as search/ff) |
| action/ffwp | (same as search/ffwp) |
| action/newest | (same as search/newest) |
| action/all | Attempt to apply the call to each file found. If any sub call succeeds the entire operation succeeds and other errors ignored. If all fail then the last error is reported. |
| create/ep | Choose the first path which is found. |
| create/mfs | Assuming the path is found to exist (ENOENT would not be returned) use the drive with the most free space available. |
| create/epmfs | If the path exists in multiple locations use the one with the most free space. Otherwise fall back to mfs. |
| create/rand | Pick a destination at random. Again the dirname of the full path must exist somewhere. |
| statfs/sumf | When reporting the size of the filesystem it will show the sum of all used but the available space will be reported as the max available across the filesystems mounted. |
| statfs/susf | As above but will report the sum of available space. Since the largest usable space is that of the filesystem with the most usable space this option is deceptive. |

**NOTE:** create is really a search for existence and then create. The 'search' policy applies to the first part. If the [dirname](http://linux.die.net/man/3/dirname) of the full path is not found to exist [ENOENT](http://linux.die.net/man/3/errno) is returned.

Usage
=====

```
$ mergerfs -o create=epmfs,search=ff,action=ff,statfs=sumf <mountpoint> <dir0>:<dir1>:<dir2>
```

| Option | Values | Default |
|--------|--------|---------|
| search | ff, ffwp, newest | ff |
| action | ff, ffwp, newest, all | ff |
| create | ep, mfs, epmfs, rand | epmfs |
| statfs | sumf, susf | sumf |

Building
========

Need to install FUSE development libraries.


```
[trapexit:~/dev/mergerfs] $ make help
usage: make
make WITHOUT_XATTR=1 - to build program without xattrs functionality
```

Runtime Settings
================

```
<mountpoint>/.mergerfs
```

There is a pseudo file available at the mountpoint which currently allows for the runtime modification of policies. The file will not show up in readdirs but can be stat'ed, read, and writen. Most other calls will fail with EPERM, EINVAL, or whatever may be appropriate for that call. Anything not understood while writing will result in EINVAL otherwise the number of bytes written will be returned.

Reading the file will result in a newline delimited list of current settings as followed:

```
[trapexit:/tmp/mount] $ cat .mergerfs
action=ff
create=epmfs
search=ff
statfs=sumf
```

Writing to the file is buffered and waits till a newline to process. Meaning echo works well.

```
[trapexit:/tmp/mount] $ echo "search=newest" >> .mergerfs
[trapexit:/tmp/mount] $ cat .mergerfs
action=ff
create=epmfs
search=newest
statfs=sumf
```

*NOTE:* offset is not supported and ignored in both read and write. There is also a safety check which limits buffered + incoming length to a max of 1024 bytes.
