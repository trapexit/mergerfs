# mergerfs - a featureful union filesystem

**mergerfs** is a
[FUSE](https://en.wikipedia.org/wiki/Filesystem_in_Userspace) based
[union filesystem](https://en.wikipedia.org/wiki/Union_mount) geared
towards simplifying storage and management of files across numerous
commodity storage devices. It is similar to **mhddfs**, **unionfs**,
and **aufs**.

## Features

* Logicially combine numerous filesystems/paths into a single
  mount point
* Combine paths of the same or different filesystems
* Ability to add or remove filesystems/paths without impacting the
  rest of the data
* Unaffected by individual filesystem failure
* Configurable file selection and creation placement
* Works with filesystems of any size
* Works with filesystems of almost any type
* Ignore read-only filesystems when creating files
* Hard link copy-on-write / CoW
* Runtime configurable
* Support for extended attributes (xattrs)
* Support for file attributes (chattr)
* Support for POSIX ACLs


## Non-features

* Read/write overlay on top of readonly filesystem like OverlayFS
* File whiteout
* RAID like parity calculation
* Redundency


## How it works

mergerfs logically merges multiple filesystem paths together. It acts
as a proxy to the underlying filesystem paths. Combining the
behaviors of some functions and being a selector for others.

When the contents of a directory are requested mergerfs combines the
list of files from each directory, deduplicating entries, and returns
that list.

When a file or directory is created a policy is run to determine which 

The
file/s or directory/s acted on or presented through mergerfs are based
on the policy chosen for that particular action. Read more about
policies below.

```
A         +      B        =       C
/disk1           /disk2           /merged
|                |                |
+-- /dir1        +-- /dir1        +-- /dir1
|   |            |   |            |   |
|   +-- file1    |   +-- file2    |   +-- file1
|                |   +-- file3    |   +-- file2
+-- /dir2        |                |   +-- file3
|   |            +-- /dir3        |
|   +-- file4        |            +-- /dir2
|                     +-- file5   |   |
+-- file6                         |   +-- file4
                                  |
                                  +-- /dir3
                                  |   |
                                  |   +-- file5
                                  |
                                  +-- file6
```

mergerfs does **not** support the copy-on-write (CoW) or whiteout
behaviors found in **aufs** and **overlayfs**. You can **not** mount a
read-only filesystem and write to it. However, mergerfs will ignore
read-only filesystems when creating new files so you can mix
read-write and read-only filesystems. It also does **not** split data
across filesystems. It is not RAID0 / striping. It is simply a union of
other filesystems.
