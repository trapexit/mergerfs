% mergerfs(1) mergerfs user manual
% Antonio SJ Musumeci <trapexit@spawn.link>
% June 9, 2014

# NAME

mergerfs - another FUSE union filesystem

# SYNOPSIS

mergerfs -ocreate=epmfs,search=ff,action=ff &lt;srcpoints&gt; &lt;mountpoint&gt;

# DESCRIPTION

mergerfs is similar to mhddfs, unionfs, and aufs. Like mhddfs in that it too uses FUSE. Like aufs in that it provides multiple policies for how to handle behavior.

Why create mergerfs when those exist? mhddfs isn't really maintained or flexible. There are also issues with running as root. aufs is more flexible but contains some hard to debug inconsistencies in behavior. Neither support file attributes ([chattr](http://linux.die.net/man/1/chattr)).

# OPTIONS

###options###

| Option | Default |
|--------|--------|
| search | ff |
| action | ff |
| create | epmfs |

###srcpoints###

The source points argument is a colon (':') delimited list of paths. To make it simplier to include multiple source points without having to modify your [fstab](http://linux.die.net/man/5/fstab) we also support [globbing](http://linux.die.net/man/7/glob).

```
$ mergerfs /mnt/disk*:/mnt/cdrom /media/drives
```

The above line will use all points in /mnt prefixed with *disk* and the directory *cdrom*.

In /etc/fstab it'd look like the following:

```
# <file system>        <mount point>  <type>         <options>    <dump>  <pass>
/mnt/disk*:/mnt/cdrom  /media/drives  fuse.mergerfs  allow_other  0       0
```

**NOTE:** the globbing is done at mount time. If a new directory is added matching the glob after the fact it will not be included.

# POLICIES

Filesystem calls are broken up into 3 categories: search, action, and create. There are also some calls which have no policy attached due to state being kept between calls. These categories can be assigned a policy which dictates how [mergerfs](http://github.com/trapexit/mergerfs) behaves. Any policy can be assigned to a category though some aren't terribly practical. For instance: rand (Random) may be useful for **create** but could lead to very odd behavior if used for **search** or **action**.

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
| ff (first found) | Given the order of the paths act on the first one found (regardless if stat would return EACCES). |
| ffwp (first found w/ permissions) | Given the order of the paths act on the first one found which you have access (stat does not error with EACCES). |
| newest (newest file) | If multiple files exist return the one with the most recent mtime. |
| mfs (most free space) | Assuming the path is found to exist (ENOENT would not be returned) use the drive with the most free space available. |
| epmfs (existing path, most free space) | If the path exists in multiple locations use the one with the most free space. Otherwise fall back to mfs. |
| rand (random) | Pick an existing destination at random. |

#### statvfs ####

It normalizes the source drives based on the fragment size and sums the number of adjusted blocks and inodes. This means you will see the combined space of all sources. Total, used, and free. The sources however are dedupped based on the drive so multiple points on the same drive will not result in double counting it's space.

**NOTE:** create is really a search for existence and then create. The 'search' policy applies to the first part. If the [dirname](http://linux.die.net/man/3/dirname) of the full path is not found to exist [ENOENT](http://linux.die.net/man/3/errno) is returned.

# BUILDING

* Need to install FUSE development libraries (libfuse-dev).
* Optionally need libattr1 (libattr1-dev).


```
[trapexit:~/dev/mergerfs] $ make help
usage: make
make XATTR_AVAILABLE=0 - to build program without xattrs functionality (auto discovered otherwise)
```

# Runtime Settings

#### /.mergerfs pseudo file ####
```
<mountpoint>/.mergerfs
```

There is a pseudo file available at the mountpoint which allows for the runtime modification of certain mergerfs options. The file will not show up in readdirs but can be stat'ed and manipulated via [{list,get,set}xattrs](http://linux.die.net/man/2/listxattr) calls.

Even if xattrs are disabled the [{list,get,set}xattrs](http://linux.die.net/man/2/listxattr) calls will still work.

The keys are:
* user.mergerfs.srcmounts
* user.mergerfs.action
* user.mergerfs.create
* user.mergerfs.search

```
[trapexit:/tmp/mount] $ xattr -l .mergerfs
user.mergerfs.srcmounts: /tmp/a:/tmp/b
user.mergerfs.action: ff
user.mergerfs.create: epmfs
user.mergerfs.search: ff

[trapexit:/tmp/mount] $ xattr -p user.mergerfs.action .mergerfs
ff

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.action ffwp .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.action .mergerfs
ffwp

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

For **user.mergerfs.srcmounts** there are several instructions available for manipulating the list. The value provided is just as the value used at mount time. A colon (':') delimited list of full path globs.

| Instruction | Description |
|--------------|-------------|
| +[list] | append |
| +<[list] | prepend |
| +>[list] | append |
| -[list] | remove all values provided |
| -< | remove first in list |
| -> | remove last in list |
| =[list] | set |
| [list] | set |

#### mergerfs file xattrs ####

While they won't show up when using [listxattr](http://linux.die.net/man/2/listxattr) mergerfs offers a number of special xattrs to query information about the files served. To access the values you will need to issue a [getxattr](http://linux.die.net/man/2/getxattr) for one of the following:

* user.mergerfs.basepath : gives you the base mount point for the file given the current search policy
* user.mergerfs.fullpath : gives you the full path of the original file given the search policy

```
[trapexit:/tmp/mount] $ ls
A B C
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.fullpath A
/mnt/a/full/path/to/A
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.basepath A
/mnt/a
```
