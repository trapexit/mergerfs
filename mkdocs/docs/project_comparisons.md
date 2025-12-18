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
similar to RAID0 but BTRFS can more explicitly place data and metadata
on multiple devices. However, like RAID0 and similar technologies if a
single device fails you may lose all data in the pool. BTRFS does have
some abilities to recover data but this is not guaranteed.


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

For more details and comparison of SnapRAID to related technologies see [https://www.snapraid.it/compare](https://www.snapraid.it/compare).


## UnRAID

* [https://unraid.net](https://unraid.net)

UnRAID is a full OS and offers a (FUSE based?) filesystem which
provides a union of filesystems like mergerfs but with the addition of
live parity calculation and storage. Outside parity calculations
mergerfs offers more features and due to the lack of real-time parity
calculation can have higher peak performance. Some users also prefer
an open source solution.

For semi-static data mergerfs + [SnapRAID](http://www.snapraid.it)
provides a similar, but not real-time, solution. NonRAID (see below)
is a fork of UnRAID's parity calculation solution and can also be used
with mergerfs.


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

ZFS is an advanced filesystem with many features including RAID5/6
like redundency.

mergerfs is very different from ZFS. mergerfs is intended to provide
flexible pooling of arbitrary filesystems (local or remote), of
arbitrary sizes, and arbitrary filesystems. Particularly in `write
once, read many` use cases such as bulk media storage. Where data
integrity and backup is managed in other ways. In those use cases ZFS
can introduce a number of costs and limitations as described
[here](http://louwrentius.com/the-hidden-cost-of-using-zfs-for-your-home-nas.html),
[here](https://markmcb.com/2020/01/07/five-years-of-btrfs/), and
[here](https://utcc.utoronto.ca/~cks/space/blog/solaris/ZFSWhyNoRealReshaping).

That said given mergerfs' flexibility it is common to see it used in
combination with ZFS where critical data is written to a RAIDZ pool
and bulk media stored on traditional filesystems pooled via mergerfs.


## ZFS AnyRAID

[ZFS
AnyRAID](https://hexos.com/blog/introducing-zfs-anyraid-sponsored-by-eshtek)
is a feature being developed for ZFS which is intended to provide more
flexibility in ZFS pools. Allowing a mix of capacity disks to have
greater capacity than traditional RAID and would allow for partial
upgrades while keeping live redundancy.

This ZFS feature, as of mid-2025, is extremely early in its development
and there are no timelines or estimates for when it may be released.


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

[9P, the Plan 9 Filesystem
Protocol,](https://en.wikipedia.org/wiki/9P_(protocol)) is a protocol
developed for the Plan 9 operating system to help expand on the Unix
idea that everything should be a file. The protocol made its way to
other systems and is still widely used. As such 9P is not directly
comparable to mergerfs but more so to FUSE which mergerfs uses. FUSE
is also a filesystem protocol (though designed for kernel <->
userspace communication rather than over a network). FUSE, even more
than the
[9P2000.L](https://github.com/chaos/diod/blob/master/protocol.md)
variant of 9P, is focused primarily on supporting Linux filesystem
features.

mergerfs leverages FUSE but could have in theory leveraged 9P with a
reduction in features.

While 9P has [extensive
usage](https://docs.kernel.org/filesystems/9p.html) in certain
situations its use in modern userland Linux systems is limited. FUSE
has largely replaced use cases that may have been implemented with 9P
servers in the past.
