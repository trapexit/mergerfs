# lazy-umount-mountpoint

* type: `BOOL`
* default: `false`
* example: `lazy-umount-mountpoint=true`

When enabled mergerfs will issue a [lazy
umount](https://man7.org/linux/man-pages/man8/umount.8.html) request
to the mount point as it starts.

```
Lazy unmount. Detach the filesystem from the file hierarchy
now, and clean up all references to this filesystem as soon as
it is not busy anymore.
```

The value of using this is in situations where you wish to change
options which can only be set at initialization or upgrade mergerfs
itself. It also helps in the case of using `mount -a` which leads to a
new instance of mergerfs being mounted due to it being unable to
recognize that it is already mounted.

In theory mergerfs could notice that an instance is already mounted at
the mount point and attempt to use the runtime interface to modify the
options and only mounting a new instance if the version has changed or
initialization time only options are changed. However, given the
relative rarity of this being an issue it isn't a priority.
