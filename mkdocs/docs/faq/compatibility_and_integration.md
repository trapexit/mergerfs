# Compatibility and Integration

## What operating systems does mergerfs support?

[Primarily Linux.](../setup/installation.md) FreeBSD is casually
supported but not well tested.

With FreeBSD certain Linux functions and FUSE features [are not
supported.](../known_issues_bugs.md#freebsd-version) In many cases the
absence will not be noticed however more advanced features will not be
available.


### Why not support MacOS?

* Last checked the support for FUSE on MacOS was in flux.
* MacOS is not an OS commonly used for NASs and other use cases
  for which mergerfs is used.
* trapexit does not own a MacOS based system.


### Why not support Windows?

[WinFSP](https://winfsp.dev/) does implement a libfuse compatible API
for Windows however mergerfs does not use libfuse (perhaps
[ironically](https://github.com/libfuse/libfuse/blob/master/AUTHORS).)
Even if mergerfs was ported to use libfuse it would require the use of
the low level API. WinFSP does not appear to support libfuse's low
level API.

Windows, while used for NAS systems more often than MacOS, is still
relatively uncommon when compared to Linux. [Drive
Pool](../project_comparisons.md#stablebits-drivepool) is a good
alternative. It may also be possible to use mergerfs with WSL.


## What filesystems can be used as branches?

ext4, btrfs, xfs, f2fs, zfs, nfs, etc.

Most any filesystem should work but there could be issues with
non-POSIX compliant filesystems such as nfs (w/ root squashing), vfat,
ntfs, cifs, exfat, etc. When directories need to be created or files
moved by mergerfs if the filesystem returns errors due to not
supporting certain POSIX filesystem features it could result in the
core functions failing.

Since mergerfs is not generally used with non-POSIX filesystems this
has not been a problem for users and there are some checks for known
edge cases but it is possible some are not accounted for. If use with a
filesystem results in issues please [file a
ticket](https://github.com/trapexit/mergerfs/issues) with the details.


## What types of storage devices does mergerfs work with?

Since mergerfs works at the filesystem level rather than device level
there is no compatibility concerns based on the physical storage
devices involved.


## Can I use mergerfs without SnapRAID? SnapRAID without mergerfs?

[https://www.snapraid.it](https://www.snapraid.it)

Yes. They are completely unrelated pieces of software that just happen
to work well together.


## How does mergerfs interact with SnapRAID recovery behavior?

SnapRAID does not choose parity layout based on directory structure or
relative path. As each file is discovered, SnapRAID fills parity space
for that file based on its size using the next available parity area.
This makes the logical parity layout depend on discovery order, file
size, and how much data already exists on each filesystem at that
time.  Filesystems with similar used space, regardless of total
capacity, are more likely to place newly added files into similar
parity positions.

Because of that, if files are changed or removed across multiple
branches and those files happen to overlap in parity space, recovery
of those deleted files may be less reliable. This is not a general
concern, is not unique to mergerfs usage, and it does not affect
filesystem failure recovery. It is specifically a concern when trying
to recover from accidental deletion of files spread across multiple
filesystems.

In practice this mostly matters when the filesystems have very similar
used space, the files are created across those filesystems around the
same time, and you later want SnapRAID to recover an accidental
deletion spanning multiple filesystems. Otherwise this is usually not
something to worry about.

If, however, this is a concern it can be beneficial to keep files on
as few filesystems as possible or otherwise keep related files
together. See the [collocation
section.](configuration_and_policies.md#how-can-i-ensure-files-are-collocated-on-the-same-branch)


## Can I use mergerfs without NonRAID? NonRAID without mergerfs?

[https://github.com/qvr/nonraid](https://github.com/qvr/nonraid)

Yes. They are completely unrelated pieces of software that just happen
to work well together.


### How does mergerfs interact with NonRAID?

There are no known special interactions or concerns between mergerfs
and NonRAID. They are separate tools and mergerfs has no known
behavior which requires any special configuration or caveats when used
with NonRAID.


## Does mergerfs support CoW / copy-on-write / writes to read-only filesystems?

Not in the sense of a filesystem like BTRFS or ZFS nor in the
overlayfs or aufs sense. It does offer a
[cow-shell](http://manpages.ubuntu.com/manpages/bionic/man1/cow-shell.1.html)
like hard link breaking (copy to temp file then rename over original)
which can be useful when wanting to save space by hardlinking
duplicate files but wish to treat each name as if it were a unique and
separate file.

If you want to write to a read-only filesystem you should look at
overlayfs. You can always include the overlayfs mount into a mergerfs
pool.


## Can mergerfs run via Docker, Podman, Kubernetes, etc.

Yes. [See installation
page.](../setup/installation.md#podman-docker-oci-containers)


## How does mergerfs interact with user namespaces?

FUSE does not have any special integration with Linux user namespaces
used by container runtime platforms like Docker, Podman, etc. The
uid/gid values passed to mergerfs will be the host level values rather
than that seen inside the container. Meaning `root` in a container
with user namespaces configured will not be `root` to mergerfs. Same
with any other uid/gid. Meaning your permissions on your branches must
work with the translated values from the id mapping.

It is generally recommended to not use user namespacing / id mapping
given the complication it introduces.


## Does mergerfs work with SELinux relabeling?

FUSE filesystems, including mergerfs, do not support per-inode
SELinux labels (see [Red Hat bug
1272868](https://bugzilla.redhat.com/show_bug.cgi?id=1272868) and the
[upstream fuse-devel
thread](https://sourceforge.net/p/fuse/mailman/fuse-devel/thread/CAN9vWDLGA4%3Dg3peKVWkX7gLRiVUUvJAQx9kiXHgqdeX6eh0MYA%40mail.gmail.com/)).
When a container runtime such as Podman or Docker is asked to relabel
a bind mount with the `:z` or `:Z` mount option, it first probes the
source for `security.selinux` xattr support. On a FUSE path that
probe returns `EOPNOTSUPP` and the runtime skips relabeling entirely.
Files inside the mount keep the `fusefs_t` type assigned by the
kernel. No tree walk is performed and no journal noise is produced at
default verbosity. The runtime logs `Labeling not supported on
"<path>"` only at debug level (for example
`podman --log-level=debug`). The flag is therefore a no-op against a
mergerfs mount; not harmful, but misleading.

Containers can still read and write a mergerfs pool without `:z`
because two independent access checks must succeed and both can be
satisfied without any relabel:

* **SELinux:** on Fedora, RHEL / CentOS Stream, and SUSE the shipped
  [container-selinux](https://github.com/containers/container-selinux)
  policy already permits the `container_t` process type to manage
  files of type `fusefs_t`. See `container_selinux(8)`.
* **DAC:** the in-container UID and GID must still be able to read
  and write the files on the underlying branches. With rootless
  Podman this is most easily arranged by aligning the container UID
  with the host owner of the branches (for example
  `--userns=keep-id`, or Quadlet `UserNS=keep-id`).

The recommendation is to omit `:z` and `:Z` on bind mounts whose
source is a mergerfs (or any other FUSE) path, and to keep them on
bind mounts whose source is a regular local filesystem (ext4, btrfs,
xfs, etc.) where relabeling does real work. On distributions whose
container-selinux policy does not already include the `fusefs_t`
allow rule, the targeted fix is to set a single label at mount time
by adding `context="system_u:object_r:container_file_t:s0"` to the
mergerfs mount options (for example in `/etc/fstab`). Files inside
the pool then appear as `container_file_t`, which container-selinux
permits `container_t` to manage everywhere. See [Red Hat's mounting
file systems
guide](https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/7/html/selinux_users_and_administrators_guide/sect-security-enhanced_linux-working_with_selinux-mounting_file_systems).
This is preferred over the much broader
`--security-opt label=disable`, which removes SELinux confinement
from the container as a whole. The trade-off of `context=` is that
host-side processes touching the mount (rsync, backup tools, web
UIs, cron jobs) see `container_file_t` rather than `fusefs_t` and
may require a small SELinux policy adjustment.

To verify on your own system:

```
podman --log-level=debug run --rm \
    -v /path/to/mergerfs:/data:z \
    alpine true 2>&1 | grep -iE 'relabel|setfilecon|Labeling'
```

Expected output is a single line of the form
`Labeling not supported on "/path/to/mergerfs"` and nothing in
`ausearch -m AVC,SELINUX_ERR` for the mount.


## Can mergerfs be used with NFS root squash?

If mergerfs is pooling a NFS mount then root squash must be disabled
as mergerfs needs to be able to have elevated privileges to do what it
does (set permissions, ownership, etc.)

If you are exporting mergerfs over NFS it is not strictly necessary to
disable root squashing but 3rd party software running as root may
fail.

See the [section on remote filesystems.](../remote_filesystems.md)


## Does inotify and fanotify work?

Yes. You can test by using
[inotifywait](https://man7.org/linux/man-pages/man1/inotifywait.1.html) or
[inotifywatch](https://man7.org/linux/man-pages/man1/inotifywatch.1.html).

However, you can not get notifications for events that occur outside
of the mergerfs mount. For instance if you are doing
[out-of-band](usage_and_functionality.md#can-filesystems-be-removed-from-the-pool-without-affecting-them)
changes it is not possible to get those events forwarded through
mergerfs. FUSE offers no mechanism to publish events and even if it
did it would require somewhat expensive
[inotify](https://man7.org/linux/man-pages/man7/inotify.7.html) or
[fanotify](https://man7.org/linux/man-pages/man7/fanotify.7.html)
watches on all branches.

Most software which has the ability to actively monitor a filesystem
(such as Plex, Emby, Jellyfin, Airsonic, etc.) are using `inotify` or
`fanotify`.

If you must add content out-of-band the only way to get real-time
updates (via inotify or fanotify) is to add the underlying branches to
the software rather than using the mergerfs mount. An alternative is
to enable regular library scanning. Plex, for instance, has "Scan my
library periodically". Additionally, if you are using software such as
Radarr, Sonarr, or Lidarr you can configure it to trigger library
updates in a number of services. This can be found under `Settings >
Connect`.
