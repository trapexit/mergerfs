# Project Comparisons

There are no solutions, only trade-offs.


## mhddfs

* [https://romanrm.net/mhddfs](https://romanrm.net/mhddfs)

mhddfs is, like mergerfs, a FUSE based filesystem which can pool
multiple filesystems together.

mhddfs had not been updated in over a decade and has known stability
and security issues. mergerfs provides a super set of mhddfs' features
and offers better performance. As of 2020 the author of mhddfs has
[moved to using mergerfs.](https://romanrm.net/mhddfs#update)

Below is an example of mhddfs and mergerfs setup to work similarly.

`mhddfs -o mlimit=4G,allow_other /mnt/drive1,/mnt/drive2 /mnt/pool`

`mergerfs -o minfreespace=4G,category.create=ff /mnt/drive1:/mnt/drive2 /mnt/pool`


## aufs

* [https://aufs.sourceforge.net](https://aufs.sourceforge.net)
* [https://en.wikipedia.org/wiki/Aufs](https://en.wikipedia.org/wiki/Aufs)

aufs, another union filesystem, is a kernel based overlay filesystem
with basic file creation placement policies.

While aufs still is maintained it failed to be included in the
mainline kernel and is no longer available in most Linux distros
making it harder to get installed for the average user.

While aufs can often offer better peak performance due to being
primarily kernel based (at least when `passthrough.io` is disabled),
mergerfs provides more configurability and is generally easier to
use. mergerfs however does not offer the overlay / copy-on-write (CoW)
features which aufs has.


## unionfs

* [https://unionfs.filesystems.org](https://unionfs.filesystems.org)

unionfs for Linux is a "stackable unification file system" which
functions like many other union filesystems. unionfs is no longer
maintained and was last released for Linux v3.14 back in 2014.

Documentation is sparse so a comparison of features is not possible
but given the lack of maintenance and support for modern kernels there
is little reason to consider it as a solution.


## unionfs-fuse

* [https://github.com/rpodgorny/unionfs-fuse](https://github.com/rpodgorny/unionfs-fuse)

unionfs-fuse is more like unionfs, aufs, and overlayfs than mergerfs
in that it offers overlay / copy-on-write (CoW) features. If you're
just looking to create a union of filesystems and want flexibility in
file/directory placement then mergerfs offers that whereas
unionfs-fuse is more for overlaying read/write filesystems over
read-only ones.

Since unionfs-fuse, as the name suggests, is a FUSE based technology
it can be used without elevated privileges that kernel solutions such
as unionfs, aufs, and overlayfs require.


## overlayfs

* [https://docs.kernel.org/filesystems/overlayfs.html](https://docs.kernel.org/filesystems/overlayfs.html)

overlayfs is effectively the successor to unionfs, unionfs-fuse, and
aufs and is widely used by Linux container platforms such as Docker and
Podman. It was developed and is maintained by the same developer who
created FUSE.

If your use case is layering a writable filesystem on top of read-only
filesystems then you should look first to overlayfs. Its feature set
however is very different from mergerfs and solve different problems.


## RAID0, JBOD, SPAN, drive concatenation, striping

* [RAID0](https://en.wikipedia.org/wiki/Standard_RAID_levels#RAID_0)
* [JBOD](https://en.wikipedia.org/wiki/Non-RAID_drive_architectures#JBOD)
* [SPAN](https://en.wikipedia.org/wiki/Non-RAID_drive_architectures#Concatenation_(SPAN,_BIG))
* [striping](https://en.wikipedia.org/wiki/Data_striping)

These are block device technologies which in some form aggregate
devices into what appears to be a singular device on which a
traditional filesystem can be used. The filesystem has no
understanding of the underlying block layout and should one of those
underlying devices fail or be removed the filesystem will be missing
that chunk which could contain critical information and the whole
filesystem may become unrecoverable. Even if the data from the
filesystem is recoverable it will take using specialized tooling to do
so.

In contrast, with mergerfs you can format devices as one normally
would or take existing filesystems and then combine them in a pool to
aggregate their storage. The failure of any one device will have no
impact on the other devices. The downside to mergerfs' technique is
the fact you don't actually have contiguous space as large as if you
used those other technologies. Meaning you can't create a file greater
than 1TB on a pool of two 1TB filesystems.


## BTRFS Single Data Profile

[BTRFS'](https://btrfs.readthedocs.io) `single` data profile is
similar to RAID0, spreading data across multiple devices but offering
no redundancy. Unlike mergerfs which pools existing filesystems at a
high level, BTRFS is a complete filesystem that manages storage
directly. If a single device fails in BTRFS single mode, you lose all
data. mergerfs takes a different approach: it pools filesystems as-is
without redundancy, so a device failure only affects data on that one
device, not the entire pool.


## RAID5, RAID6

* [RAID5](https://en.wikipedia.org/wiki/Standard_RAID_levels#RAID_5)
* [RAID6](https://en.wikipedia.org/wiki/Standard_RAID_levels#RAID_6)

RAID5, RAID6, and similar technologies aggregate multiple devices at
the block level and offer 1 or more live parity calculations that can
allow the pool to continue to run should 1 device fail and rebuild the
data once the device is replaced.

mergerfs offers no parity or redundancy features so in that regard the
technologies are not comparable. [SnapRAID](https://www.snapraid.it)
or [nonraid](https://github.com/qvr/nonraid) can be used in
combination with mergerfs to provide redundancy. Unlike traditional
RAID5 or RAID6 SnapRAID works with drives of different sizes and can
have more than 2 parity drives. However, parity calculations are not
done in real-time. However, nonraid is realtime.

For more details and comparison of SnapRAID to related technologies
see [https://www.snapraid.it/compare](https://www.snapraid.it/compare).


## UnRAID

* [https://unraid.net](https://unraid.net)

UnRAID is a full OS and offers a (FUSE based?) filesystem which
provides a union of filesystems like mergerfs but with the addition of
live parity calculation and storage. Outside parity calculations
mergerfs offers more features and due to the lack of real-time parity
calculation can have higher peak performance. For some users mergerfs
being open source is also preferable.

For semi-static data mergerfs + [SnapRAID](http://www.snapraid.it)
provides a similar, but not real-time,
solution. [NonRAID](https://github.com/qvr/nonraid) (see below) is a
fork of UnRAID's parity calculation solution and can also be used with
mergerfs.


## NonRAID

* [https://github.com/qvr/nonraid](https://github.com/qvr/nonraid)

[NonRAID](https://github.com/qvr/nonraid#how-it-works) is a fork of
UnRAID's storage array kernel driver. Effectively it is like RAID5 at
the block device layer without treating the collection of devices as a
single device. Each device gets a virtual block device associated with
it which can be formatted with the filesystem of the user's choice and
will be individually accessible. Similar to SnapRAID but real-time.

Given each device has its own filesystem like a traditional setup it can
be used with mergerfs for those looking for a unified view.


## ZFS

* [https://en.wikipedia.org/wiki/ZFS](https://en.wikipedia.org/wiki/ZFS)

ZFS is an advanced filesystem that combines storage pooling,
snapshots, data integrity checking, and built-in RAID-like redundancy
all in one package. It offers features like transparent compression,
encryption, and automatic data repair.

Unlike mergerfs which focuses on simple pooling of existing
filesystems, ZFS is a complete filesystem that manages storage
directly. ZFS requires more resources and planning to set up, making
it better suited for systems where data protection and integrity are
critical priorities. mergerfs is simpler and lighter weight, making it
ideal for users who want to add storage incrementally without the
overhead of ZFS.

Both technologies can aggregate multiple drives into a single pool,
but they approach the problem differently. ZFS integrates everything
into the filesystem, while mergerfs pools existing filesystems. For
write-once, read-many workloads like bulk media storage where backup
and data integrity is managed separately, mergerfs is often the better
choice due to lower resource usage and simpler management.

It's [common to see](https://perfectmediaserver.com) both used
together: critical data stored on a ZFS pool with redundancy, while
bulk media is pooled across regular filesystems using mergerfs.


## ZFS AnyRAID

[ZFS
AnyRAID](https://hexos.com/blog/introducing-zfs-anyraid-sponsored-by-eshtek)
is a feature being developed for ZFS to provide more flexibility in
pools by allowing mixed-capacity disks while maintaining live
redundancy.

If released, ZFS AnyRAID would offer somewhat similar flexibility to
mergerfs' pooling approach, but with built-in redundancy. mergerfs
already provides this flexibility today: add drives of any size at any
time with no redundancy overhead. If you need redundancy with that
flexibility, ZFS AnyRAID could be an option when available; until
then, mergerfs remains the simpler choice for mixed-capacity pooling
with redendancy and integrity available via SnapRAID and/or NonRAID.


## Bcachefs

* [https://bcachefs.org](https://bcachefs.org)

Bcachefs is a modern copy-on-write filesystem for Linux designed for
multi-device storage aggregation. It combines the data integrity and
crash-safety features of ZFS with the performance optimization of
high-performance filesystems like XFS and ext4.

Bcachefs is fundamentally a multi-device filesystem. It can aggregate
multiple physical disks of arbitrary sizes into a single logical
filesystem, allowing data to be automatically distributed across
devices. It includes built-in replication capabilities similar to RAID
mirroring and parity RAID, with automatic failover and recovery.

The filesystem features comprehensive data protection including full
data and metadata checksumming, copy-on-write architecture that
eliminates write holes, and atomic transactions that ensure crash
safety. It also provides snapshots, transparent compression (LZ4,
gzip, Zstandard), and transparent encryption.

Bcachefs provides intelligent tiered storage where data can be
automatically placed across devices based on access patterns. Faster
devices (NVMe, SSD) can be designated as foreground or promotion
targets for hot data, while slower devices (HDD) serve as background
storage for cold data. This enables efficient use of heterogeneous
storage without manual configuration.

Like ZFS, bcachefs is most suitable for use cases where integrated
storage management, data redundancy, and reliability are important. It
differs from mergerfs in that it is a complete filesystem rather than
a pooling layer, providing built-in redundancy and automatic data
placement rather than relying on external tools or the properties of
underlying filesystems.

Bcachefs is under active development and as of early 2026 should be
considered beta quality. It is suitable for testing and non-critical
deployments, but careful evaluation is recommended before use in
production systems.


## StableBit's DrivePool

* [https://stablebit.com](https://stablebit.com)

DrivePool is a commercial filesystem pooling technology for
Windows. If you are looking to use Windows then DrivePool is a good
option. Functionally the two projects work a bit
differently. DrivePool always writes to the filesystem with the most
free space and later rebalances. mergerfs does not currently offer
rebalance but chooses a branch at file/directory create
time. DrivePool's rebalancing can be done differently in any directory
and has file pattern matching to further customize the
behavior. mergerfs, not having rebalancing does not have these
features, but similar features are planned for mergerfs v3. DrivePool
has builtin file duplication which mergerfs does not natively support
(but can be done via an external program.)

There are a lot of misc differences between the two projects but most
features in DrivePool can be replicated with external tools in
combination with mergerfs.


## Windows Storage Spaces

* [https://learn.microsoft.com/en-us/windows-server/storage/storage-spaces/overview](https://learn.microsoft.com/en-us/windows-server/storage/storage-spaces/overview)

Windows Storage Spaces is Microsoft's software-defined storage
platform for Windows Server and Windows Pro. It aggregates multiple
physical disks into logical storage pools with built-in redundancy
options (mirroring, parity) and automatic performance optimization
through tiered caching.

Storage Spaces is Windows-only and requires more planning upfront due
to redundancy options and RAID-like features. mergerfs is
Linux-focused and takes a simpler approach, pooling existing
filesystems without built-in redundancy. If you need a Windows
solution with integrated data protection, Storage Spaces is a good
choice. For Linux users seeking lightweight pooling without redundancy
overhead, mergerfs is the better option.


## Plan9 binds

* [https://en.wikipedia.org/wiki/Plan_9_from_Bell_Labs#Union_directories_and_namespaces](https://en.wikipedia.org/wiki/Plan_9_from_Bell_Labs#Union_directories_and_namespaces)

Plan9 has the native ability to bind multiple paths/filesystems
together to create a setup similar to simplified union
filesystem. Such bind mounts choose files in a "first found" manner
based on the order they are configured similar to mergerfs' `ff`
policy. Similarly, when creating a file it will be created on the
first directory in the union.

Plan 9 isn't a widely used OS so this comparison is mostly academic.


## SnapRAID pooling

[SnapRAID](https://www.snapraid.it/manual) has a pooling feature which
creates "a read-only virtual view of all the files in your array using
symbolic links."

As mentioned in the description this "view" is just the creation of
the same directory layout with symlinks to all files. This means that
reads (and writes) to files are at native speeds but limited in that
it can not practically be used as a target for writing new files and
is only updated when `snapraid pool` is run. Note that some software
treat symlinks differently than regular files. For instance some
backup software will skip symlinks by default.

mergerfs has the feature [symlinkify](config/symlinkify.md) which
provides a similar behavior but is more flexible in that it is not
read-only. That said there can still be some software that won't like
that kind of setup.


## rclone union

rclone's [union](https://rclone.org/union) backend allows you to
create a union of multiple rclone backends and was inspired by
[mergerfs](https://rclone.org/union/#behavior-policies). Given rclone
knows more about the underlying backend than mergerfs could it can be
more efficient than creating a similar union with `rclone mount` and
mergerfs.

However, it is not uncommon to see users setup rclone mounts and
combine them with local or other remote filesystems using mergerfs
given the differing feature sets and focuses of the two projects.


## distributed filesystems

* AFS
* Ceph/CephFS
* GlusterFS
* LizardFS
* MooseFS
* etc.

Distributed remote filesystems come in many forms. Some offering POSIX
filesystem compliance and some not. Some providing remote block
devices or object stores on which a POSIX or POSIX-like filesystem is
built on top of. Some which are effectively distributed union
filesystems with duplication.

These filesystems almost always require a significant amount of
compute to run well and are typically deployed on their own
hardware. Often in an "orchestrators" + "workers" configuration across
numerous nodes. This limits their usefulness for casual and homelab
users. There could also be issues with network congestion and general
performance if using a single network and that network is slower than
the storage devices.

While possible to use a distributed filesystem to combine storage
devices (and typically to provide redundancy) it will require a more
complicated setup and more compute resources than mergerfs (while also
offering a different set of capabilities.)


## 9P

[9P](https://en.wikipedia.org/wiki/9P_(protocol)) is a filesystem
protocol from the Plan 9 operating system. While historically
important, it's not directly relevant for users looking to pool
filesystems. 9P is a network protocol, whereas mergerfs uses FUSE,
which is designed specifically for implementing filesystems in
userspace on Linux. FUSE has become the modern standard for this
purpose and better supports Linux filesystem features. For practical
filesystem pooling, mergerfs is the standard choice on Linux.
