# Migration

## Unraid

mergerfs and a parity-protected Unraid array can both present
directories from multiple filesystems as a single view, but they treat
storage membership very differently. With mergerfs, membership is a
property of its **configuration**: branches are paths and can be added
or removed at runtime without modifying the underlying filesystems.
With Unraid's main array, membership is part of the **array state**:
adding a data disk to an array with active parity normally requires
clearing and formatting it, and removing one normally requires parity
to be rebuilt.

Those array-level requirements, not the User Share filesystem itself,
are what shape migrations in either direction. When moving to mergerfs,
existing data filesystems can usually be reused as-is. When moving to
Unraid, populated filesystems can normally only be brought into the
array before parity is activated. See [Project
Comparisons](project_comparisons.md#unraid) for the comparison itself.

| Operation | mergerfs | Unraid array with parity |
| --- | --- | --- |
| Add an empty filesystem | Add its mount path as a branch | Clear if necessary, then format and add |
| Add a populated filesystem | Add its mount path; data is immediately visible | Not normally possible once parity is active without destroying its contents |
| Mix filesystem types | Any filesystem the host can mount | Supported array filesystem types only |
| Mix device sizes | Arbitrary | Data disks cannot exceed the parity disk size |
| Change membership while mounted | Supported through the [runtime interface](runtime_interface.md) | Array configuration normally requires stopping and restarting the array |
| Remove a filesystem and retain it unchanged | Remove the branch | Possible, but normal array removal requires parity to be rebuilt |
| Remove a filesystem without recalculating parity | Not applicable | Requires zeroing the disk, destroying its filesystem |

### Migrating from Unraid to mergerfs

Unraid array data disks each hold a complete, individually formatted
filesystem. `shfs` only unions their top-level directories, so there
is no striped or parity-dependent layout to unwind. After cleanly
stopping the array, unencrypted data filesystems can be mounted under
Linux and their mount points used directly as mergerfs branches. The
files do not need to be rewritten merely because the pooling
implementation changed.

For [encrypted Unraid disks](https://docs.unraid.net/unraid-os/system-administration/secure-your-server/securing-your-data/),
securely retain the existing passphrase or key file before retiring
Unraid. Open each disk's existing LUKS container with `cryptsetup`
using those credentials, then mount the inner filesystem through its
`/dev/mapper/` device and use that mount point as a branch. Do not
reformat the disk or create a new LUKS container. Without a working
passphrase or key file, the encrypted data cannot be accessed.

```
Unraid:

    disk1 ─┐
    disk2 ─┼─> shfs ─> /mnt/user
    disk3 ─┘
       +
    parity

mergerfs:

    disk1 ─┐
    disk2 ─┼─> mergerfs ─> /mnt/storage
    disk3 ─┘
```

Unraid's convention of representing shares as identically named
top-level directories also maps naturally to mergerfs' union
semantics.

* The parity disk is not a data filesystem and cannot become a branch
  as-is. It can be reformatted as another data disk if parity
  protection is no longer desired, or a separate parity solution such
  as [SnapRAID](https://www.snapraid.it) or
  [NonRAID](https://github.com/qvr/nonraid) can be used alongside
  mergerfs if redundancy is still wanted.
* If migrating incrementally while keeping the remaining Unraid array
  operational, the normal Unraid disk-removal and parity-rebuild
  requirements still apply to each disk removed.
* If the entire array is being retired at once, those intermediate
  Unraid parity rebuilds are unnecessary. Stop the array cleanly and
  use the existing data filesystems as branches.
* Named pools need care. A pool backed by a single filesystem can be
  mounted and used as one branch. A multi-device Btrfs or ZFS pool is
  not equivalent to a collection of independent Unraid array disks: it
  can either remain intact and be mounted as one mergerfs branch, or
  its data must be migrated elsewhere before its component devices can
  be repurposed as independent filesystems.

### Migrating from mergerfs to Unraid

mergerfs places no special metadata on its branches, so their
underlying filesystems remain unchanged. Direct reuse as Unraid array
data disks requires local disks with a supported filesystem **and a
partition layout accepted by the installed Unraid version**. A
supported filesystem type alone is not sufficient.

In particular, mergerfs can use a filesystem formatted directly on a
whole device, such as `/dev/sda`, without a partition table. Unraid
array data disks require a filesystem within a compatible partition.
Such unpartitioned filesystems cannot be imported directly: back up
their data, let Unraid partition and format the disks, then restore
the files. Creating a partition table over the existing filesystem is
not a safe conversion. Check Unraid's
[partitioning guidance](https://docs.unraid.net/unraid-os/troubleshooting/faq/)
before planning a no-copy migration.

Current versions of Unraid support importing populated disks when
constructing a new array, or after resetting the array configuration,
provided that the populated data disks are added **before parity is
activated**. Such a migration can therefore often be performed without
copying the data:

1. Stop mergerfs and cleanly unmount the branch filesystems.
2. Create or reset the Unraid array configuration.
3. Assign all populated mergerfs disks as data disks while no parity
   disk is active.
4. Verify that the existing filesystems and data are recognized
   correctly. If a populated disk is reported as unmountable, stop and
   investigate; do not format it, as formatting destroys its contents.
5. Assign a parity disk at least as large as the largest data disk.
6. Build parity over the completed array.

Once parity has been established, that convenient path is effectively
closed. Adding another populated mergerfs disk to the parity-protected
array would require Unraid to clear the disk, destroying the existing
filesystem. A staged migration in which populated mergerfs disks are
added one at a time therefore generally requires either copying their
contents into the existing array first or resetting/reconstructing the
Unraid configuration so that all populated data disks are present
before parity is established.

Additional array-level constraints to keep in mind:

* Only filesystem types supported by Unraid can be assigned as array
  data disks.
* No array data disk may be larger than the parity disk.
* Array configuration changes normally require stopping and restarting
  the array. Removing a data disk normally requires a parity rebuild,
  and Unraid's parity-preserving removal procedure works by erasing
  the filesystem and zeroing the disk, so it is unsuitable when the
  goal is to retain that filesystem for use elsewhere.

When migrating into individual **Unraid array data slots**, the
following configurations require copying data to suitable array
filesystems rather than reusing the source storage directly:

* Remote filesystems or filesystem types Unraid does not support.
* Storage that cannot be presented as a single array device, such as
  multi-device Btrfs or ZFS pools or `mdadm` arrays.

This restriction does not rule out retaining storage as a named pool.
A compatible multi-device ZFS pool can be
[imported into Unraid](https://docs.unraid.net/unraid-os/advanced-configurations/optimize-storage/zfs-storage/#importing-zfs-pools-created-on-other-systems)
intact, without copying its data. Stop mergerfs and cleanly export the
pool on the source system. With the Unraid array stopped, use **Add
Pool**, assign all of the pool's devices (including support vdevs),
set **File System** to **Auto**, and start the array to import it.
If it is not recognized, investigate rather than format its devices.
The imported pool remains separate from the parity-protected array;
its redundancy comes from its ZFS layout, not Unraid array parity.

A branch pointing at a subdirectory does **not** by itself require a
data copy: eligibility depends on its underlying disk and filesystem.
For example, importing compatible disks that contributed
`/mnt/disk1/media` and `/mnt/disk2/media` preserves their contents, and
Unraid combines the top-level `media` directories into a
[User Share](https://docs.unraid.net/unraid-os/using-unraid-to/manage-storage/shares/#user-shares).
Review the desired share layout; deeper branch directories may need
to be reorganized within their existing filesystems rather than
copied to new storage.

### Summary

mergerfs treats storage membership as a property of its
**configuration**, while a parity-protected Unraid array treats it as
part of the **array state**. Moving an existing filesystem into or out
of mergerfs normally does not alter that filesystem at all. Moving
populated filesystems into or out of an active Unraid parity array can
require clearing disks, rebuilding parity, moving data, or
reconstructing the array configuration.

For semi-static data, mergerfs + SnapRAID provides pooling and parity
while retaining the ability to add and remove independently formatted
filesystems without modifying them. NonRAID provides an alternative
real-time parity layer which can likewise be combined with mergerfs.
