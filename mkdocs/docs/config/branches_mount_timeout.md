# branches-mount-timeout

Default: `0`

mergerfs is compatible with any path, whether it be a mount point or a
directory on your [root
filesystem.](https://en.wikipedia.org/wiki/Root_directory) It doesn’t
require branch paths to be mounted or to match a specific filesystem
at startup, and it operates effectively without needing details about
the intended filesystem. This flexibility eliminates the need to
manage mount ordering, which is particularly advantageous on modern
systems where filesystems are mounted asynchronously, resulting in
unpredictable mount sequences. While
[systemd](https://www.freedesktop.org/software/systemd/man/latest/systemd.mount.html)
offers a way to enforce mount dependencies using the
[x-systemd.requires=PATH](https://www.freedesktop.org/software/systemd/man/latest/systemd.mount.html#x-systemd.requires=)
option in /etc/fstab, applying this to every branch path is both
cumbersome and susceptible to errors.

`branches-mount-timeout` will cause mergerfs to wait for all branches
to become "mounted." The logic to determine "mounted" is as follows.

1. It is mounted if the branch directory has the extended attribute
   `user.mergerfs.branch`
2. It is mounted if the branch directory contains a file named
   `.mergerfs.branch`
3. It is **not** mounted if the branch directory has the extended
   attribute `user.mergerfs.branch_mounts_here`
4. It is **not** mounted if the branch directory contains
   a file named `.mergerfs.branch_mounts_here`
5. It is mounted if the `st_dev` value of the mergerfs mountpoint is
   different from the branch path.

**NOTE:** If on a `systemd` based system and using `fstab` it is a
good idea to set the mount option
[x-systemd.mount-timeout](https://www.freedesktop.org/software/systemd/man/latest/systemd.mount.html#x-systemd.mount-timeout=)
to some value longer than `branches-mount-timeout`.


## branches-mount-timeout-fail

Default: `false`

When set to `true` mergerfs will fail entirely after
`branches-mount-timeout` expires without all branches being
mounted. If set to `false` it will simply ignore the mount status of
the branches and continue on. The details will be
[logged](../error_handling_and_logging.md).
