# Runtime Interface

`mergerfs` has runtime interfaces allowing users to query certain
filesystem information, get and set config, and trigger certain
activities while it is running.

The interface is provided via the POSIX extended attributes filesystem
API which is a namespaced `key=value` pair store associated with a
file. Since `mergerfs` primarily uses `key=value` pairs for config it
fits well and is a known and reasonably well supported API.

There are two targets for `xattr` calls. One is a pseudo file used for
getting and setting config and issuing certain commands. The other are
files found on the filesystem for querying certain `mergerfs` specific
information about them.


## .mergerfs pseudo file

```
<mountpoint>/.mergerfs
```

`mergerfs` provides this pseudo file for the runtime modification of
certain options and issuing commands. The file will not show up in
`readdir` but can be `stat`'ed and viewed/manipulated via
[listxattr](http://linux.die.net/man/2/listxattr),
[getxattr](https://linux.die.net/man/2/getxattr), and
[setxattr](https://linux.die.net/man/2/setxattr) calls.

Any changes made at runtime are **NOT** persisted. If you wish for
values to persist they must be included as options wherever you
configure the mounting of mergerfs (/etc/fstab, systemd, etc.).


### Command Line Tooling

Extended attributes is prevelant enough that there are common tools
available for interacting with them.

In Debian / Ubuntu distributions you can get the tools
[getfattr](https://linux.die.net/man/1/getfattr) and
[setfattr](https://linux.die.net/man/1/setfattr) from
[attr](https://linux.die.net/man/5/attr) package.

```
$ sudo apt-get install attr
```


### Config

#### Keys

Use `getfattr -d /mountpoint/.mergerfs` to see all supported
configuration keys. It is effectively the same as the
[options](config/options.md) prefixed with `user.mergerfs.`. Some are
informational or only can be set at startup and therefore read-only.

Example: option `cache.files` would be `user.mergerfs.cache.files`.


#### Values

Same as the [command line options](config/options.md).


#### Getting

`getfattr -n user.mergerfs.branches /mountpoint/.mergerfs`

`ENOATTR` will be returned if the key doesn't exist as normal with
[getxattr](https://linux.die.net/man/2/getxattr).


#### Setting

`setfattr -n user.mergerfs.branches -v VALUE /mountpoint/.mergerfs`

[setxattr](https://linux.die.net/man/2/setxattr) will return `EROFS`
(Read-only filesystem) on read-only keys. `ENOATTR` will be returned
if the key does not exist. If the value attempting to be set is not
valid `EINVAL` will be returned.


#### user.mergerfs.branches

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
check config". This is done to ensure the user understands the
situation and continue to be able to access the xattr interface.


#### Example

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

### Commands

There are a number of commands / behaviors which can be triggerd by
writing ([setfattr](https://linux.die.net/man/1/setfattr),
[setxattr](https://linux.die.net/man/2/setxattr)) particular xattr
keys of `/mountpoint/.mergerfs`. These keys do not show up in key
listings.

Commands can take an argument however currently no command uses or
allows a value.

| Key | Value | Description |
| --- | ----- | ----------- |
| user.mergerfs.cmd.gc | (empty) |Trigger a thorough garbage collection of certain pools of resources. The xattr value is not used. |
| user.mergerfs.cmd.gc1 | (empty) | Trigger a simple garbage collection of certain pools of resources. This is also done on a timer. |
| user.mergerfs.cmd.invalidate-all-nodes | (empty) | Attempts to invalidate FUSE file nodes. Primarily used for debugging. |

```
[trapexit:/mnt/mergerfs] $ setfattr -n user.mergerfs.cmd.gc /mnt/mergerfs/.mergerfs
[trapexit:/mnt/mergerfs] $ journalctl -t mergerfs | tail -n1
Jul 20 15:36:18 hostname mergerfs[1931348]: running basic garbage collection
```


### file / directory xattrs

There is certain information `mergerfs` knows or calculates about a
file that can be useful in building tooling or debugging.

The keys below can be queried (`getfattr -n key`,
[getxattr](http://linux.die.net/man/2/getxattr)) to get the described
information. These keys will not show up in any listing (`getfattr
-d`, [listxattr](https://linux.die.net/man/2/listxattr)). Attempting
to set them will result in an error.

| Key | Return Value |
| --- | ------------ |
| user.mergerfs.basepath | The base mount point for the file given the current `getattr` policy. |
| user.mergerfs.relpath | The relative path of the file from the perspective of the mount point. |
| user.mergerfs.fullpath | The full path of the original file given the `getattr` policy. |
| user.mergerfs.allpaths | A NULL ('\0') separated list of full paths to all files found. |


```
[trapexit:/mnt/mergerfs] $ getfattr -n user.mergerfs.fullpath test
user.mergerfs.fullpath="/mnt/a/test"
[trapexit:/mnt/mergerfs] $ getfattr -n user.mergerfs.allpaths .
user.mergerfs.allpaths="/mnt/a\000/mnt/b\000/mnt/c"
```
