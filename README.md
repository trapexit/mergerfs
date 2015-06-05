% mergerfs(1) mergerfs user manual
% Antonio SJ Musumeci <trapexit@spawn.link>
% 2015-02-05

# NAME

mergerfs - another FUSE union filesystem

# SYNOPSIS

mergerfs -o&lt;options&gt; &lt;srcpoints&gt; &lt;mountpoint&gt;

# DESCRIPTION

mergerfs is similar to mhddfs, unionfs, and aufs. Like mhddfs in that it too uses FUSE. Like aufs in that it provides multiple policies for how to handle behavior.

Why create mergerfs when those exist? mhddfs isn't really maintained or flexible. There are also issues with running as root. aufs is more flexible but contains some hard to debug inconsistencies in behavior. Neither support file attributes ([chattr](http://linux.die.net/man/1/chattr)).

# OPTIONS

###options###

`defaults` is a shortcut to `big_writes`, `auto_cache`, `atomic_o_trunc`, `splice_read`, `splice_write`, and `splice_move`. These options seem to provide the best performance.

All [FUSE](http://fuse.sourceforge.net) functions which have a category (see below) are option keys. The syntax being `func.<func>=<policy>`.

To set all function policies in a category use `category.<category>=<policy>` such as `category.create=mfs`.

They are evaluated in the order listed so if the options are `func.rmdir=rand,category.action=ff` the `action` category setting will override the `rmdir` setting.

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

Filesystem calls are broken up into 3 categories: action, create, search. There are also some calls which have no policy attached due to state being kept between calls. These categories can be assigned a policy which dictates how [mergerfs](http://github.com/trapexit/mergerfs) behaves. Any policy can be assigned to a category though some aren't terribly practical. For instance: rand (Random) may be useful for **create** but could lead to very odd behavior if used for **search**.

#### Functional  classifications ####
| FUSE Function | Class |
|-------------|---------|
| access      | search  |
| chmod       | action  |
| chown       | action  |
| create      | create  |
| fallocate   | N/A     |
| fgetattr    | N/A     |
| fsync       | N/A     |
| ftruncate   | N/A     |
| getattr     | search  |
| getxattr    | search  |
| ioctl       | N/A*    |
| link        | action  |
| listxattr   | search  |
| mkdir       | create  |
| mknod       | create  |
| open        | search  |
| read        | N/A     |
| readdir     | N/A     |
| readlink    | search  |
| release     | N/A     |
| removexattr | action  |
| rename      | action  |
| rmdir       | action  |
| setxattr    | action  |
| statfs      | N/A     |
| symlink     | create  |
| truncate    | action  |
| unlink      | action  |
| utimens     | action  |
| write       | N/A     |

`ioctl` behaves differently if its acting on a directory. It'll use the `getattr` policy to find and open the directory before issuing the `ioctl`. In other cases where something may be searched (to confirm a directory exists across all source mounts) then `getattr` will be used.

#### Policy descriptions ####
| Policy | Description |
|--------------|-------------|
| ff (first found) | Given the order of the paths act on the first one found (regardless if stat would return EACCES). |
| ffwp (first found w/ permissions) | Given the order of the paths act on the first one found which you have access (stat does not error with EACCES). |
| newest (newest file) | If multiple files exist return the one with the most recent mtime. |
| mfs (most free space) | Assuming the path is found to exist (ENOENT would not be returned) use the drive with the most free space available. |
| epmfs (existing path, most free space) | If the path exists in multiple locations use the one with the most free space. Otherwise fall back to mfs. |
| rand (random) | Pick an existing destination at random. |
| all | Applies action to all found. For searches it will behave like first found `ff`. |

#### Defaults ####
| Category | Policy |
|----------|--------|
| action   | all    |
| create   | epmfs  |
| search   | ff     |

#### readdir ####

[readdir](http://linux.die.net/man/3/readdir) is very different from most functions in this realm. It certainly could have it's own set of policies to tweak its behavior. At this time it provides a simple `first found` merging of directories and file found. That is: only the first file or directory found for a directory is returned. Given how FUSE works though the data representing the returned entry comes from `getattr`.

It could be extended to offer the ability to see all files found. Perhaps concatinating `#` and a number to the name. But to really be useful you'd need to be able to access them which would complicate file lookup.

#### statvfs ####

[statvfs](http://linux.die.net/man/2/statvfs) normalizes the source drives based on the fragment size and sums the number of adjusted blocks and inodes. This means you will see the combined space of all sources. Total, used, and free. The sources however are dedupped based on the drive so multiple points on the same drive will not result in double counting it's space.

**NOTE:** Since we can not (easily) replicate the atomicity of an `mkdir` or `mknod` without side effects those calls will first do a scan to see if the file exists and then attempts a create. This means there is a slight race condition. Worse case you'd end up with the directory or file on more than one mount.

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

##### Keys #####
* user.mergerfs.srcmounts
* user.mergerfs.category.action
* user.mergerfs.category.create
* user.mergerfs.category.search
* user.mergerfs.func.access
* user.mergerfs.func.chmod
* user.mergerfs.func.chown
* user.mergerfs.func.create
* user.mergerfs.func.getattr
* user.mergerfs.func.getxattr
* user.mergerfs.func.link
* user.mergerfs.func.listxattr
* user.mergerfs.func.mkdir
* user.mergerfs.func.mknod
* user.mergerfs.func.open
* user.mergerfs.func.readlink
* user.mergerfs.func.removexattr
* user.mergerfs.func.rename
* user.mergerfs.func.rmdir
* user.mergerfs.func.setxattr
* user.mergerfs.func.symlink
* user.mergerfs.func.truncate
* user.mergerfs.func.unlink
* user.mergerfs.func.utimens

##### Example #####

```
[trapexit:/tmp/mount] $ xattr -l .mergerfs
user.mergerfs.srcmounts: /tmp/a:/tmp/b
user.mergerfs.category.action: all
user.mergerfs.category.create: epmfs
user.mergerfs.category.search: ff
user.mergerfs.func.access: ff
user.mergerfs.func.chmod: all
user.mergerfs.func.chown: all
user.mergerfs.func.create: epmfs
user.mergerfs.func.getattr: ff
user.mergerfs.func.getxattr: ff
user.mergerfs.func.link: all
user.mergerfs.func.listxattr: ff
user.mergerfs.func.mkdir: epmfs
user.mergerfs.func.mknod: epmfs
user.mergerfs.func.open: ff
user.mergerfs.func.readlink: ff
user.mergerfs.func.removexattr: all
user.mergerfs.func.rename: all
user.mergerfs.func.rmdir: all
user.mergerfs.func.setxattr: all
user.mergerfs.func.symlink: epmfs
user.mergerfs.func.truncate: all
user.mergerfs.func.unlink: all
user.mergerfs.func.utimens: all

[trapexit:/tmp/mount] $ xattr -p user.mergerfs.category.search .mergerfs
ff

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.category.search ffwp .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.category.search .mergerfs
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

##### Extra Details #####

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

Categories and funcs take a policy as described in the previous section. When reading funcs you'll get the policy string. However, with categories you'll get a coma separated list of policies for each type found. For example: if all search functions are `ff` except for `access` which is `ffwp` the value for `user.mergerfs.category.search` will be `ff,ffwp`.

#### mergerfs file xattrs ####

While they won't show up when using [listxattr](http://linux.die.net/man/2/listxattr) mergerfs offers a number of special xattrs to query information about the files served. To access the values you will need to issue a [getxattr](http://linux.die.net/man/2/getxattr) for one of the following:

* user.mergerfs.basepath: the base mount point for the file given the current search policy
* user.mergerfs.relpath: the relative path of the file from the perspective of the mount point
* user.mergerfs.fullpath: the full path of the original file given the search policy
* user.mergerfs.allpaths: a NUL ('\0') separated list of full paths to all files found

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
