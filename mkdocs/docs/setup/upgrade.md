# Upgrade

mergerfs can be upgraded live by mounting on top of the previous
instance. Simply install the new version of mergerfs and follow the
instructions below.

Run mergerfs again or if using `/etc/fstab` (or systemd mount file)
call for it to mount again. Existing open files and directories will
continue to work fine though they won't see any differences that the
new version would provide since it is still using the previous
instance. If you plan on changing settings with the new mount you
should / could apply those before mounting the new version.

```
$ sudo mount /mnt/mergerfs
$ mount | grep mergerfs
media on /mnt/mergerfs type mergerfs (rw,relatime,user_id=0,group_id=0,default_permissions,allow_other)
media on /mnt/mergerfs type mergerfs (rw,relatime,user_id=0,group_id=0,default_permissions,allow_other)
```

A problem with this approach is that the underlying instance will
continue to run even if the software using it stop or are
restarted. To work around this you can use a "lazy umount". Before
mounting over top the mount point with the new instance of mergerfs
issue: `umount -l <mergerfs_mountpoint>`. Or you can let mergerfs do
it by setting the option `lazy-umount-mountpoint=true`.

If the intent is to change settings at runtime then the [runtime
interface](../runtime_interfaces.md) should be used.
