# Terminology

* `disk`, `drive`, `disk drive`: A [physical data storage
  device](https://en.wikipedia.org/wiki/Disk_storage). Such as a hard
  drive or solid-state drive. Usually requires the use of a filesystem
  to be useful. mergerfs does not deal with disks.
* `filesystem`: Lowlevel software which provides a way to organize data
  and provide access to said data in a standard way. A filesystem is a
  higher level abstraction that may or may not be stored on a
  disk. mergerfs deals exclusively with filesystems.
* `path`: A location within a filesystem. mergerfs can work with any
  path within a filesystem and not simply the root.
* `branch`: A base path used in a mergerfs pool. mergerfs can
  accomidate multiple paths pointing to the same filesystem.
* `pool`: The mergerfs mount. The union of the branches. The instance
  of mergerfs. You can mount multiple mergerfs pools. Even with the
  same branches.
* `relative path`: The path in the pool relative to the branch and
  mount. `foo/bar` is the relative path of mergerfs mount
  `/mnt/mergerfs/foo/bar`.
* `function`: A filesystem call such as `open`, `unlink`, `create`,
  `getattr`, `rmdir`, etc. The requests your software make to the
  filesystem.
* `category`: A collection of functions based on basic behavior
  (action, create, search).
* `policy`: The algorithm used to select a file or files when
  performing a function.
* `path preservation`: Aspect of some policies which includes checking
  the path for which a file would be created.
* `out-of-band`:
  [out-of-band](https://en.wikipedia.org/wiki/Out-of-band) in our
  context refers to interacting with the underlying filesystem
  directly instead of going through mergerfs (or NFS or Samba).
