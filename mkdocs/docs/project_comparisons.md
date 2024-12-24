# Project Comparisons

## mhddfs

mhddfs had not been updated in over a decade and has known stability
and security issues. mergerfs provides a superset of mhddfs' features
and offers better performance.

Below is an example of mhddfs and mergerfs setup to work similarly.

`mhddfs -o mlimit=4G,allow_other /mnt/drive1,/mnt/drive2 /mnt/pool`

`mergerfs -o minfreespace=4G,category.create=ff /mnt/drive1:/mnt/drive2 /mnt/pool`


## aufs

aufs is abandoned and no longer available in most Linux distros.

While aufs can offer better peak performance mergerfs provides more
configurability and is generally easier to use. mergerfs however does
not offer the overlay / copy-on-write (CoW) features which aufs has.


## Linux unionfs

FILL IN


## unionfs-fuse

unionfs-fuse is more like aufs than mergerfs in that it offers overlay
/ copy-on-write (CoW) features. If you're just looking to create a
union of filesystems and want flexibility in file/directory placement
then mergerfs offers that whereas unionfs-fuse is more for overlaying
read/write filesystems over read-only ones. Largely unionfs-fuse has
been replaced by overlayfs.


## overlayfs

overlayfs is similar to aufs and unionfs-fuse in that it also is
primarily used to layer a read/write filesystem over one or more
read-only filesystems. It does not have the ability to spread
files/directories across numerous filesystems. It is the successor to
unionfs, unionfs-fuse, and aufs and widely used by container platforms
such as Docker.

If your usecase is layering a writable filesystem on top of readonly
filesystems then you should look first to overlayfs.


## RAID0, JBOD, drive concatenation, striping

With simple JBOD / drive concatenation / stripping / RAID0 a single
drive failure will result in full pool failure. mergerfs performs a
similar function without the possibility of catastrophic failure and
the difficulties in recovery. Drives may fail but all other
filesystems and their data will continue to be accessible.

The main practical difference with mergerfs is the fact you don't
actually have contiguous space as large as if you used those other
technologies. Meaning you can't create a 2TB file on a pool of 2 1TB
filesystems.


## UnRAID

UnRAID is a full OS and offers a (FUSE based?) filesystem which
provides a union of filesystems like mergerfs but with the addition of
live parity calculation and storage. Outside parity calculations
mergerfs offers more features and due to the lack of realtime parity
calculation can have high peak performance. Some users also prefer an
open source solution.

For semi-static data mergerfs + [SnapRaid](http://www.snapraid.it)
provides a similar solution.


## ZFS

mergerfs is very different from ZFS. mergerfs is intended to provide
flexible pooling of arbitrary filesystems (local or remote), of
arbitrary sizes, and arbitrary filesystems. Primarily in `write once, read
many` usecases such as bulk media storage. Where data integrity and
backup is managed in other ways. In those usecases ZFS can introduce a
number of costs and limitations as described
[here](http://louwrentius.com/the-hidden-cost-of-using-zfs-for-your-home-nas.html),
[here](https://markmcb.com/2020/01/07/five-years-of-btrfs/), and
[here](https://utcc.utoronto.ca/~cks/space/blog/solaris/ZFSWhyNoRealReshaping).


## StableBit's DrivePool

DrivePool works only on Windows so not as common an alternative as
other Linux solutions. If you want to use Windows then DrivePool is a
good option. Functionally the two projects work a bit
differently. DrivePool always writes to the filesystem with the most
free space and later rebalances. mergerfs does not currently offer
rebalance but chooses a branch at file/directory create
time. DrivePool's rebalancing can be done differently in any directory
and has file pattern matching to further customize the
behavior. mergerfs, not having rebalancing does not have these
features, but similar features are planned for mergerfs v3. DrivePool
has builtin file duplication which mergerfs does not natively support
(but can be done via an external script.)

There are a lot of misc differences between the two projects but most
features in DrivePool can be replicated with external tools in
combination with mergerfs.

Additionally, DrivePool is a closed source commercial product vs
mergerfs a ISC licensed open source project.


## Plan9 binds

Plan9 has the native ability to bind multiple paths/filesystems
together which can be compared to a simplified union filesystem. Such
bind mounts choose files in a "first found" in the order they are
listed similar to mergerfs' `ff` policy. File creation is limited
to... FILL ME IN. REFERENCE DOCS.
