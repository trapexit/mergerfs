# Compatibility and Integration

## Can I use mergerfs without SnapRAID? SnapRAID without mergerfs?

Yes. They are completely unrelated pieces of software that just happen
to work well together.


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

Yes. With Docker you'll need to include `--cap-add=SYS_ADMIN
--device=/dev/fuse --security-opt=apparmor:unconfined` or similar with
other container runtimes. You should also be running it as root or
given sufficient caps to allow mergerfs to change user and group
identity as well as have root like filesystem permissions. This
ability is critical to how mergerfs works.

Also, as mentioned by [hotio](https://hotio.dev/containers/mergerfs),
with Docker you should probably be mounting with `bind-propagation`
set to `slave`.


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


## Can mergerfs be used with NFS root squash?

If mergerfs is pooling a NFS mount then root squash should be disabled
as mergerfs needs to be able to have elevated privilages to do what it
does.

If you are exporting mergerfs over NFS then it is not really necessary.

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
