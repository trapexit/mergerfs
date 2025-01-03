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
given sufficient caps to change user and group identity as well as
have root like filesystem permissions.

Keep in mind that you **MUST** consider identity when using
containers. For example: supplemental groups will be picked up from
the container unless you properly manage users and groups by sharing
relevant /etc files or by using some other means to share identity
across containers. Similarly, if you use "rootless" containers and user
namespaces to do uid/gid translations you **MUST** consider that while
managing shared files.

Also, as mentioned by [hotio](https://hotio.dev/containers/mergerfs),
with Docker you should probably be mounting with `bind-propagation`
set to `slave`.
