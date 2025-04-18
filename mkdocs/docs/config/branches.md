# branches

The `branches` argument is a colon (':') delimited list of paths to be
pooled together. It does not matter if the paths are on the same or
different filesystems nor does it matter [the filesystem
type](../faq/compatibility_and_integration.md/#what-filesystems-can-be-used-as-branches)
(within reason). Used and available space metrics will not be
duplicated for paths on the same filesystem and any features which
aren't supported by the underlying filesystem (such as file attributes
or extended attributes) will return the appropriate errors.

Branches currently have two options which can be set. A
[mode](#branch-mode) which impacts whether or not the branch is
included in a policy calculation and a individual
[minfreespace](#minfreespace) value. The values are set by
prepending an `=` at the end of a branch designation and using commas
as delimiters. Example: `/mnt/drive=RW,1234`


### branch mode

- RW: (read/write) - Default behavior. Will be eligible in all policy
  categories.
- RO: (read-only) - Will be excluded from `create` and `action`
  policies. Same as a read-only mounted filesystem would be (though
  faster to process).
- NC: (no-create) - Will be excluded from `create` policies. You can't
  create on that branch but you can change or delete.


### minfreespace

Same purpose and syntax as the [global option](minfreespace.md) but
specific to the branch. Defaults to the global value.


### globbing

To make it easier to include multiple branches mergerfs supports
[globbing](http://linux.die.net/man/7/glob). **The globbing tokens
MUST be escaped when using via the shell else the shell itself will
apply the glob itself.**

```
# mergerfs /mnt/hdd\*:/mnt/ssd /media
```

The above line will use all directories in /mnt prefixed with **hdd**
as well as **ssd**.

To have the pool mounted at boot or otherwise accessible from related
tools use `/etc/fstab`.

```
# <file system>        <mount point>  <type>    <options>             <dump>  <pass>
/mnt/hdd*:/mnt/ssd    /media          mergerfs  minfreespace=16G      0       0
```

**NOTE:** The globbing is done at mount or when updated using the
runtime API. If a new directory is added matching the glob after the
fact it will not be automatically included.

A convenient way to configure branches is to use
[symlinks](../quickstart.md/#etcfstab-w-config-file).


## branch setup

mergerfs does not require any special setup of branch paths in order
to be used however here are some suggestions.


### layout

When you have a large collection of storage devices, types of storage,
and types of interconnects it can be useful to have a verbose naming
convention. My preference is to create a directory within `/mnt/` for
each major type of storage device and/or interconnect. `hdd` for
traditional hard drives; `ssd` for slower SATA based SSDs; `nvme` for
M.2 or U.2 NVME SSDs; `remote` for NFS, SMB, sshfs, etc.

The mount points within those directories are named after the size of
the storage and its serial number in the case of physical storage and
the hostname and remote filesystem for remote.

```
$ ls -lh /mnt/
total 16K
drwxr-xr-x 8 root root 4.0K Aug 18  2024 hdd
drwxr-xr-x 6 root root 4.0K Oct  8  2024 nvme
drwxr-xr-x 3 root root 4.0K Aug 24  2024 remote
drwxr-xr-x 3 root root 4.0K Jul 14  2024 ssd

$ ls -lh /mnt/hdd/
total 8K
d--------- 2 root root 4.0K Apr 14 15:58 10T-01234567
d--------- 2 root root 4.0K Apr 12 20:51 20T-12345678

$ ls -lh /mnt/nvme/
total 8K
d--------- 2 root root 4.0K Apr 14 16:00 1T-ABCDEFGH
d--------- 2 root root 4.0K Apr 14 23:24 1T-BCDEFGHI

$ ls -lh /mnt/remote/
total 8K
d--------- 2 root root 4.0K Apr 12 20:23 foo-sshfs
d--------- 2 root root 4.0K Apr 12 20:24 bar-nfs

# You can find the serial number of a drive using lsblk
$ lsblk -d -o NAME,PATH,SIZE,SERIAL
NAME    PATH           SIZE SERIAL
sda     /dev/sda       9.1T 01234567
sdb     /dev/sdb      18.2T 12345678
nvme0n1 /dev/nvme0n1 953.9G ABCDEFGH
nvme1n1 /dev/nvme1n1 953.9G BCDEFGHI
```


### permissions and mode

#### mount points

To ensure the directory is **only** used as a point to mount another
filesystem it is good to lock it down as much as possible. Be sure to
do this **before** mounting a filesystem to it.

```
$ sudo chown root:root /mnt/hdd/10T-XYZ
$ sudo chmod 0000 /mnt/hdd/10T-XYZ
$ sudo setfattr -n user.mergerfs.branch_mounts_here
$ sudo chattr +i /mnt/hdd/10T-XYZ
```

The extended attribute `user.mergerfs.branch_mounts_here` is used by
the [branches-mount-timeout](branches_mount_timeout.md) option to
recognize whether or not a mergerfs branch path points to the intended
filesystem.

The `chattr` is likely to only work on EXT{2,3,4} filesystems but will
restrict even `root` from modifying the directory or its content.


#### mounted filesystems

For those new to Linux, intending to be the primary individual logged
into the system, or simply want to simplify permissions it is
recommended to set the root of mounted filesystems like `/tmp/` is set
to. Owned by `root`, `ugo+rwx` and [sticky
bit](https://en.wikipedia.org/wiki/Sticky_bit) set.

This must be done **after** mounting the filesystem to the target
mount point.

```
$ sudo chown root:root /mnt/hdd/10T-SERIALNUM
$ sudo chmod 1777 /mnt/hdd/10T-SERIALNUM
$ sudo setfattr -n user.mergerfs.branch /mnt/hdd/10T-SERIALNUM
```


### formatting

While even less relevant to mergerfs than the details above the topic
does com up regularly with mergerfs users. When it comes to
partitioning and formatting it is suggested to keep things
simple. Most users of mergerfs will be using the whole drive capacity
and as such have a singular partition and filesystem on that
partition. Something many people don't realize is that the partition
is not necessary and actually can become problematic.

Rather than creating paritions just format the block device. Let's use
`/dev/sda` as an example.

```
$ lsblk -d -o NAME,PATH,SERIAL /dev/sda
NAME    PATH         SERIAL
sda     /dev/sda     01234567

$ sudo mkfs.ext4 -m0 -L 01234567 /dev/sda
mke2fs 1.47.0 (5-Feb-2023)
Discarding device blocks: done
Creating filesystem with 262144 4k blocks and 65536 inodes
Filesystem UUID: ede8bf96-9aff-464e-a4fb-a4ab8a197cd7
Superblock backups stored on blocks:
        32768, 98304, 163840, 229376

Allocating group tables: done
Writing inode tables: done
Creating journal (8192 blocks): done
Writing superblocks and filesystem accounting information: done
```

You can then use the serial number as the identifier in `fstab`.

```
# <file system>  <mount point>           <type>  <options>  <dump>  <pass>
LABEL=01234567   /mnt/hdd/10TB-01234567  auto    nofail     0       2
```

Benefits of doing it this way?

* One less thing (partitions) to configure and worry about.
* Guarantees alignment of blocks
* Makes it easier to use the drive with different enclosures. Some
  SATA to USB adapters use 4K blocks while the drive itself is using
  512b. If you create partitions with one block size and move the
  drive to a controller that uses the other the offset on the device
  where the filesystem starts will be misinterpreted. It is possible
  to manually fix this but it isn't well documented and avoidable.
