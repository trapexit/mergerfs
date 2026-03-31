# Project Comparisons

There are no solutions, only trade-offs.


## mhddfs

* [https://romanrm.net/mhddfs](https://romanrm.net/mhddfs)

mhddfs is, like mergerfs, a FUSE-based filesystem which can pool
multiple filesystems together.

mhddfs had not been updated in over a decade and has known stability
and security issues. mergerfs provides a superset of mhddfs' features
and offers better performance. As of 2020, the author of mhddfs has
[moved to using mergerfs.](https://romanrm.net/mhddfs#update)

* **Better than mergerfs for:** legacy setups that already depend on
  mhddfs-specific behavior and do not need newer features.
* **Worse than mergerfs for:** most modern deployments due to maintenance
  status, feature coverage, and reliability.

Below is an example of mhddfs and mergerfs setup to work similarly.

```
mhddfs -o mlimit=4G,allow_other /mnt/drive1,/mnt/drive2 /mnt/pool

mergerfs -o minfreespace=4G,category.create=ff /mnt/drive1:/mnt/drive2 /mnt/pool
```


## aufs

* [https://aufs.sourceforge.net](https://aufs.sourceforge.net)
* [https://en.wikipedia.org/wiki/Aufs](https://en.wikipedia.org/wiki/Aufs)

aufs, another union filesystem, is a kernel-based overlay filesystem
with basic file creation placement policies.

Compared with mergerfs, aufs focuses on kernel-level overlay and
copy-on-write behavior, while mergerfs focuses on userspace pooling of
existing filesystems with richer branch placement policies.

aufs failed to be included in the mainline kernel and is no longer
available in most Linux distros, making it harder to get installed for
the average user. Development has been largely dormant for years.

While aufs can often offer better peak performance due to being
primarily kernel-based (at least when `passthrough.io` is disabled in
mergerfs), mergerfs provides more configurability and is generally
easier to use. mergerfs, however, does not offer the overlay /
copy-on-write (CoW) features which aufs has.

* **Better than mergerfs for:** kernel-level overlay/CoW workloads where
  that behavior is required.
* **Worse than mergerfs for:** users needing a widely packaged,
  actively-maintained, easy-to-deploy pooling solution.


## unionfs

* [https://unionfs.filesystems.org](https://unionfs.filesystems.org)

unionfs for Linux is a "stackable unification file system" which
functions like many other union filesystems. It was last released for
Linux v3.14 back in 2014.

Compared with mergerfs, unionfs is an older kernel union filesystem
implementation while mergerfs is a modern, actively-maintained
FUSE-based pooling filesystem with broader policy controls.

Documentation is sparse so a comparison of features is not possible
but given the lack of maintenance and support for modern kernels there
is little reason to consider it as a solution.

* **Better than mergerfs for:** historical or legacy environments that
  already depend on this specific kernel unionfs behavior.
* **Worse than mergerfs for:** practical modern deployments due to
  limited maintenance, aging kernel support, and sparse documentation.


## unionfs-fuse

* [https://github.com/rpodgorny/unionfs-fuse](https://github.com/rpodgorny/unionfs-fuse)

unionfs-fuse is a FUSE-based union filesystem focused on overlay
behavior, including copy-on-write (CoW) style layering of writable
content over read-only content.

Compared with mergerfs, unionfs-fuse's primary feature is
overlay/CoW-style layering, while mergerfs' primary feature is
policy-based pooling and placement across multiple regular writable
filesystems.

Since unionfs-fuse, as the name suggests, is a FUSE-based technology
it can be used without elevated privileges that kernel solutions such
as unionfs, aufs, and overlayfs require.

* **Better than mergerfs for:** overlay-style unions with copy-on-write
  semantics in userspace.
* **Worse than mergerfs for:** flexible create-policy-based pooling of
  many writable filesystems.


## overlayfs

* [https://docs.kernel.org/filesystems/overlayfs.html](https://docs.kernel.org/filesystems/overlayfs.html)

overlayfs is effectively the functional successor to unionfs,
unionfs-fuse, and aufs and is widely used by Linux container platforms
such as Docker and Podman. Both overlayfs and FUSE were originally
created by Miklos Szeredi.

If your use case is layering a writable filesystem on top of read-only
filesystems then you should look first to overlayfs. Its feature set
however is very different from mergerfs and solves different problems.

* **Better than mergerfs for:** container and image-layering workflows
  where copy-on-write semantics are central.
* **Worse than mergerfs for:** heterogeneous pooling use cases that need
  flexible branch selection policies across many existing filesystems.


## RAID0, JBOD, SPAN, drive concatenation, striping

* [RAID0](https://en.wikipedia.org/wiki/Standard_RAID_levels#RAID_0)
* [JBOD](https://en.wikipedia.org/wiki/Non-RAID_drive_architectures#JBOD)
* [SPAN](https://en.wikipedia.org/wiki/Non-RAID_drive_architectures#Concatenation_(SPAN,_BIG))
* [striping](https://en.wikipedia.org/wiki/Data_striping)

These are block device technologies which, in some form, aggregate
devices into what appears to be a single device on which a traditional
filesystem can be used. The filesystem has no understanding of the
underlying block layout and, should one of those underlying devices
fail or be removed, the filesystem will be missing that chunk, which
could contain critical information, and the whole filesystem may
become unrecoverable. Even if the data from the filesystem is
recoverable, doing so may require specialized tooling.

In contrast, with mergerfs you can format devices as you normally
would, or take existing filesystems and combine them in a pool to
aggregate their storage. The failure of any one device will have no
impact on the other devices. The downside to mergerfs' approach is
that you do not actually have contiguous space as large as you would
with those other technologies. That means you cannot create a file
larger than 1TB on a pool of two 1TB filesystems.

* **Better than mergerfs for:** creating one contiguous block device and
  maximizing single-file size/performance characteristics.
* **Worse than mergerfs for:** failure isolation and straightforward
  per-disk recovery when a device dies.


## RAID5, RAID6

* [RAID5](https://en.wikipedia.org/wiki/Standard_RAID_levels#RAID_5)
* [RAID6](https://en.wikipedia.org/wiki/Standard_RAID_levels#RAID_6)

RAID5, RAID6, and similar technologies aggregate multiple devices at
the block level and provide one or more live parity schemes that can
allow the pool to continue to run should one device fail and rebuild
the data once the device is replaced.

mergerfs offers no parity or redundancy features so in that regard the
technologies are not comparable. [SnapRAID](https://www.snapraid.it)
or [NonRAID](https://github.com/qvr/nonraid) can be used in
combination with mergerfs to provide redundancy. Unlike traditional
RAID5 or RAID6 SnapRAID works with drives of different sizes and can
have more than 2 parity drives. Parity calculations with SnapRAID are
not done in real-time but NonRAID's are.

For more details and comparison of SnapRAID to related technologies
see [https://www.snapraid.it/compare](https://www.snapraid.it/compare).

* **Better than mergerfs for:** live parity protection and continued array
  operation through configured disk failures.
* **Worse than mergerfs for:** incremental mixed-size pooling with simple
  per-filesystem independence.


## LVM

* [https://en.wikipedia.org/wiki/Logical_Volume_Manager_(Linux)](https://en.wikipedia.org/wiki/Logical_Volume_Manager_(Linux))
* [https://sourceware.org/lvm2](https://sourceware.org/lvm2)

LVM is a block-level volume management system for Linux. It combines
physical devices into volume groups and creates logical volumes on top,
with features like snapshots, resizing, and thin provisioning.

Compared with mergerfs, LVM operates below the filesystem while mergerfs
operates above filesystems. With LVM, filesystems are created on logical
volumes and do not know about the underlying disks. If a linear or
striped logical volume spans multiple disks and one disk fails, the
filesystem can become unreadable. With mergerfs, each disk typically has
its own independent filesystem, so a single disk failure affects only the
data on that disk.

* **Better than mergerfs for:** block-level features like thin
  provisioning, snapshots, and online resizing.
* **Worse than mergerfs for:** incremental mixed-disk pooling where
  per-disk failure isolation and per-disk native access are priorities.


## mdadm

* [https://en.wikipedia.org/wiki/Mdadm](https://en.wikipedia.org/wiki/Mdadm)
* [https://raid.wiki.kernel.org/index.php/A_guide_to_mdadm](https://raid.wiki.kernel.org/index.php/A_guide_to_mdadm)

`mdadm` is Linux userspace tooling for configuring and managing software
RAID arrays provided by the kernel `md` driver. It can build RAID0/1/4/5/6/10
and related layouts, monitor arrays, and handle rebuild workflows.

Like RAID technologies discussed above, `mdadm` arrays aggregate devices
at the block layer and present one virtual device to a filesystem. That
can provide performance and/or redundancy depending on RAID level, but it
also means the filesystem depends on the array layout. mergerfs does not
provide block-level RAID; it pools already-formatted filesystems and can
be combined with tools like SnapRAID or NonRAID when redundancy is needed.

* **Better than mergerfs for:** kernel software RAID with real-time
  redundancy and array-level performance tuning.
* **Worse than mergerfs for:** flexible mixed-size drive pooling where
  per-filesystem independence and simple incremental expansion matter.


## device mapper

* [https://en.wikipedia.org/wiki/Device_mapper](https://en.wikipedia.org/wiki/Device_mapper)
* [https://docs.kernel.org/admin-guide/device-mapper](https://docs.kernel.org/admin-guide/device-mapper)

Device mapper is a kernel framework for creating virtual block devices
from one or more underlying devices. Technologies such as LVM,
`dm-crypt`, `dm-cache`, `dm-thin`, and multipath are built on top of it.

Because device mapper works at the block layer, it is not directly
comparable to mergerfs. It is complementary: you can use device mapper
for block-level functionality (encryption, caching, thin provisioning,
etc.) and then place filesystems on those devices, which mergerfs can
later pool if desired.

* **Better than mergerfs for:** block-layer transformations such as
  transparent encryption, caching, and provisioning features.
* **Worse than mergerfs for:** presenting one unified pathname-level view
  across many independent, already-formatted filesystems.


## UnRAID

* [https://unraid.net](https://unraid.net)

UnRAID is a full OS platform that offers pooled storage with integrated
real-time parity. Compared with mergerfs, its main advantage is that it
bundles pooling and parity in one product, while mergerfs is a
standalone Linux pooling filesystem without built-in redundancy. For
some users mergerfs' Linux distro-agnostic and fully open-source
approach is also preferable.

For semi-static data, mergerfs + [SnapRAID](https://www.snapraid.it)
provides a similar, but not real-time, solution.
[NonRAID](https://github.com/qvr/nonraid) (see below) is a
fork of UnRAID's parity calculation solution and can also be used with
mergerfs.

* **Better than mergerfs for:** turnkey pooling plus integrated real-time
  parity in one product.
* **Worse than mergerfs for:** users who prefer a Linux distro-agnostic,
  fully open-source component approach.


## NonRAID

* [https://github.com/qvr/nonraid](https://github.com/qvr/nonraid)

[NonRAID](https://github.com/qvr/nonraid#how-it-works) is a fork of
UnRAID's storage array kernel driver. Effectively it is like RAID5 at
the block device layer without treating the collection of devices as a
single device. Each device gets a virtual block device associated with
it which can be formatted with the filesystem of the user's choice and
will be individually accessible. Similar to SnapRAID but real-time.

Compared with mergerfs, NonRAID adds kernel-level real-time parity while
still allowing per-disk filesystems, whereas mergerfs focuses on pooling
views and placement policies without parity.

Given each device has its own filesystem like a traditional setup it can
be used with mergerfs for those looking for a unified view.

* **Better than mergerfs for:** real-time parity protection without
  traditional RAID striping behavior.
* **Worse than mergerfs for:** users who want the simplest setup and do
  not need parity, since it adds operational complexity.


## ZFS

* [https://en.wikipedia.org/wiki/ZFS](https://en.wikipedia.org/wiki/ZFS)

ZFS is an advanced filesystem that combines storage pooling,
snapshots, data integrity checking, and built-in RAID-like redundancy
all in one package. It offers features like transparent compression,
encryption, and automatic data repair.

Compared with mergerfs, ZFS is a full storage stack that manages disks
directly with integrated integrity and redundancy features, while
mergerfs is a lightweight FUSE layer that pools existing filesystems
without built-in redundancy.

* **Better than mergerfs for:** integrated data integrity, snapshots,
  checksumming, and built-in redundancy.
* **Worse than mergerfs for:** low-overhead incremental pooling of
  existing filesystems with minimal planning.


## ZFS AnyRAID

* [https://github.com/openzfs/zfs/discussions/16773](https://github.com/openzfs/zfs/discussions/16773)

[ZFS AnyRAID](https://github.com/openzfs/zfs/discussions/16773)
([announcement](https://hexos.com/blog/introducing-zfs-anyraid-sponsored-by-eshtek))
is a feature under development for ZFS intended to provide more
flexibility in pools by allowing mixed-capacity disks while
maintaining live redundancy.

If released, ZFS AnyRAID would offer some of the flexibility associated
with mergerfs-style mixed-capacity expansion, but with built-in
redundancy inside ZFS. mergerfs already provides that mixed-size,
add-drives-anytime flexibility today, albeit without built-in
redundancy. If you need redundancy with that flexibility, ZFS AnyRAID
could become an option when available; until then, mergerfs remains the
simpler currently available choice for mixed-capacity pooling, with
redundancy and integrity available via SnapRAID and/or NonRAID.

* **Better than mergerfs for:** users who want mixed-capacity flexibility
  and built-in live redundancy in one filesystem stack.
* **Worse than mergerfs for:** users who need mature availability now and
  the simplest add-drives-anytime workflow without redundancy overhead.


## Bcachefs

* [https://bcachefs.org](https://bcachefs.org)

Bcachefs is a modern copy-on-write filesystem for Linux designed for
multi-device storage aggregation. It includes integrated features such
as checksumming, snapshots, compression, encryption, and redundancy.

Compared with mergerfs, bcachefs is a full integrated filesystem with
built-in checksumming, snapshots, compression, and redundancy, while
mergerfs is a lightweight pooling layer over existing filesystems.
Bcachefs is still evolving and should be evaluated carefully for
production use.

* **Better than mergerfs for:** integrated CoW filesystem features,
  checksumming, snapshots, compression, and built-in multi-device
  redundancy.
* **Worse than mergerfs for:** conservative production setups prioritizing
  maturity, simplicity, and broad long-term deployment stability.


## Btrfs Single Data Profile

* [https://btrfs.readthedocs.io](https://btrfs.readthedocs.io)

[Btrfs'](https://btrfs.readthedocs.io) `single` data profile is a
multi-device Btrfs mode that can spread data across drives without
adding redundancy.

Compared with mergerfs, Btrfs `single` is managed as one integrated
filesystem, while mergerfs pools separate existing filesystems. In
Btrfs `single`, losing one device can impact the whole filesystem;
with mergerfs, loss is typically limited to data on the failed disk.

* **Better than mergerfs for:** integrated multi-device management in one
  filesystem namespace.
* **Worse than mergerfs for:** users who value per-disk failure isolation
  and easy independent recovery.


## StableBit's DrivePool

* [https://stablebit.com](https://stablebit.com)

DrivePool is a commercial storage pooling product for Windows that
combines multiple drives into a single volume and includes balancing
and optional duplication capabilities.

Compared with mergerfs, DrivePool emphasizes integrated balancing and
duplication features in a Windows-first product, while mergerfs
emphasizes Linux FUSE pooling with policy-based create placement and
external-tool composability.

* **Better than mergerfs for:** Windows users needing built-in
  rebalancing and native file duplication features.
* **Worse than mergerfs for:** Linux-native deployments and users who
  prefer open-source tooling and composable external utilities.


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

* **Better than mergerfs for:** Windows environments needing integrated
  pooling, resilience, and tiering in the platform stack.
* **Worse than mergerfs for:** Linux hosts and lightweight pooling where
  minimal overhead and per-filesystem independence are preferred.


## Plan 9 binds

* [https://en.wikipedia.org/wiki/Plan_9_from_Bell_Labs#Union_directories_and_namespaces](https://en.wikipedia.org/wiki/Plan_9_from_Bell_Labs#Union_directories_and_namespaces)

Plan 9 has a native bind mechanism that can compose paths and
filesystems into a single namespace view, including union-like
directory behavior.

Compared with mergerfs, Plan 9 binds are an OS-native namespace feature
with simple ordered behavior, while mergerfs provides Linux-focused
filesystem pooling with a larger set of configurable branch policies.

Plan 9 isn't a widely used OS so this comparison is mostly academic.

* **Better than mergerfs for:** native Plan 9 environments where
  namespace composition is a core OS feature.
* **Worse than mergerfs for:** Linux users due to ecosystem relevance and
  practical deployment availability.


## SnapRAID pooling

* [https://www.snapraid.it/manual](https://www.snapraid.it/manual)

[SnapRAID](https://www.snapraid.it/manual) has a pooling feature which
creates "a read-only virtual view of all the files in your array using
symbolic links."

Compared with mergerfs, SnapRAID pooling is a symlink-generated,
read-only view refreshed by SnapRAID operations, while mergerfs is a
live mounted filesystem that supports normal read/write behavior.

As mentioned in the description, this "view" is just the creation of
the same directory layout with symlinks to all files. This means that
reads (and writes) to files are at native speeds, but it is not
practical to use as a target for writing new files, and it is only
updated when `snapraid pool` is run. Note that some software treats
symlinks differently than regular files. For instance, some backup
software will skip symlinks by default.

mergerfs has the feature [symlinkify](config/symlinkify.md) which
provides a similar behavior but is more flexible in that it is not
read-only. That said there can still be some software that won't like
that kind of setup.

* **Better than mergerfs for:** read-only, native-speed directory views
  of SnapRAID-managed content.
* **Worse than mergerfs for:** general read/write pooled filesystem use
  and always-live view updates.


## rclone union

* [https://rclone.org/union](https://rclone.org/union)

rclone's [union](https://rclone.org/union) backend allows you to
create a union of multiple rclone backends and was inspired by
[mergerfs](https://rclone.org/union/#behavior-policies). Because
rclone knows more about the underlying backends than mergerfs can, it
can be more efficient than creating a similar union with `rclone mount`
and mergerfs.

Compared with mergerfs, rclone union is backend-aware and optimized for
cloud/remote storage abstractions, while mergerfs is optimized for local
POSIX filesystem pooling and branch policies.

However, it is not uncommon to see users set up rclone mounts and
combine them with local or other remote filesystems using mergerfs
given the differing feature sets and focuses of the two projects.

* **Better than mergerfs for:** remote-backend-heavy workloads where
  backend-aware behavior improves cloud storage handling.
* **Worse than mergerfs for:** local POSIX pooling scenarios where direct
  filesystem semantics and broad local tool compatibility matter.


## distributed filesystems

* [AFS](https://www.openafs.org)
* [Ceph/CephFS](https://ceph.io)
* [GlusterFS](https://www.gluster.org)
* [LizardFS](https://lizardfs.com)
* [MooseFS](https://moosefs.com)
* etc.

Distributed remote filesystems come in many forms. Some offer POSIX
filesystem compliance and some do not. Some provide remote block
devices or object stores on which a POSIX or POSIX-like filesystem is
built. Some are effectively distributed union filesystems with
duplication.

Compared with mergerfs, distributed filesystems focus on multi-node
storage, replication, and cluster behavior, while mergerfs focuses on
single-host local pooling of existing filesystems.

* **Better than mergerfs for:** multi-node or geographically distributed
  workloads with built-in replication and high availability.
* **Worse than mergerfs for:** single-host and homelab pooling where low
  overhead and operational simplicity are key.


## nofs

* [https://github.com/chapmanjacobd/nofs](https://github.com/chapmanjacobd/nofs)

nofs is a tool that provides mergerfs-like functionality for combining
multiple filesystems/directories into a unified view but does so
entirely through subcommands which replicate traditional POSIX
commands rather than providing a proper filesystem. It takes a lot of
inspiration from mergerfs in that it supports policy-based branch
selection (mfs, ff, pfrd, epmfs, and others), read-only and no-create
branch modes, and recreates POSIX-like commands such as `ls`, `find`,
`which`, `cp`, `mv`, and `rm`.

Compared with mergerfs, nofs is command-driven rather than a mounted
filesystem, so mergerfs is the option for transparent system-wide
pooling through normal POSIX filesystem calls.

Given its design nofs is not suited for general usage as third-party
applications will not be able to take advantage of the unioning
behavior it offers. It is primarily for more simple situations where
something like mergerfs is unable to be used.

* **Better than mergerfs for:** constrained environments where a FUSE
  mount is not possible but command-style union behavior is acceptable.
* **Worse than mergerfs for:** transparent system-wide filesystem pooling
  usable by arbitrary third-party applications.


## policyfs

* [https://policyfs.org](https://policyfs.org)
* [https://github.com/hieutdo/policyfs](https://github.com/hieutdo/policyfs)

policyfs is a Linux FUSE storage daemon that unifies multiple storage
paths under a single mountpoint, similarly to mergerfs. Its
distinguishing features are explicit path-pattern-based routing rules
for reads and writes, and an optional SQLite metadata index that can
serve `readdir` and `getattr` operations without spinning up HDDs.

Compared with mergerfs, policyfs emphasizes explicit path-based routing
and optional metadata indexing, while mergerfs emphasizes a broad,
policy-driven pooling model with longer operational history and wider
adoption.

* **Better than mergerfs for:** explicit path-based routing and optional
  metadata indexing to reduce HDD spin-ups.
* **Worse than mergerfs for:** users prioritizing project maturity,
  adoption, and long-established operational behavior.


## Greyhole

* [https://www.greyhole.net](https://www.greyhole.net)
* [https://github.com/gboudreau/Greyhole](https://github.com/gboudreau/Greyhole)

Greyhole is an open-source storage pooling application that works via
Samba. Rather than implementing a FUSE filesystem, it hooks into Samba
VFS to intercept file operations and distribute files across multiple
drives, with optional per-share redundancy.

Compared with mergerfs, Greyhole is Samba-centric and includes
integrated share-level redundancy, while mergerfs is a general-purpose
Linux FUSE mount for local pooling via standard POSIX filesystem calls.

* **Better than mergerfs for:** Samba-centric environments that need
  built-in per-share redundancy in the same tool.
* **Worse than mergerfs for:** general-purpose local filesystem pooling
  outside Samba and heavy small-file churn workloads.


## 9P

* [https://en.wikipedia.org/wiki/9P_(protocol)](https://en.wikipedia.org/wiki/9P_(protocol))

[9P](https://en.wikipedia.org/wiki/9P_(protocol)) is a filesystem
protocol from the Plan 9 operating system used to access files over a
network between systems.

Compared with mergerfs, 9P is a network filesystem protocol, while
mergerfs is a local Linux FUSE filesystem for pooling directories and
filesystems under one mount with branch policies.

* **Better than mergerfs for:** lightweight network filesystem protocol
  use between compatible systems.
* **Worse than mergerfs for:** local Linux filesystem pooling because it
  does not provide mergerfs-like branch policies or pooling semantics.
