# mergerfs - a featureful union filesystem

**mergerfs** is a
[FUSE](https://en.wikipedia.org/wiki/Filesystem_in_Userspace) based
[union filesystem](https://en.wikipedia.org/wiki/Union_mount) geared
towards simplifying storage and management of files across numerous
commodity storage devices. It is similar to **mhddfs**, **unionfs**,
and **aufs**.

## Features

* Logically combine numerous filesystems/paths into a single
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

* Read/write overlay on top of read-only filesystem like OverlayFS
* File whiteout
* RAID like parity calculation
* Redundancy
* Splitting of files across branches


## How it works

mergerfs logically merges multiple filesystem paths together. It acts
as a proxy to the underlying filesystem paths. Combining the behaviors
of some functions and being a selector for others.

When the contents of a directory are requested mergerfs combines the
list of files from each directory, deduplicating entries, and returns
that list.

When a file or directory is created a policy is first run to determine
which branch will be selected for the creation.

For functions which change attributes or remove the file the behavior
may be applied to all instances found.

Read more about [policies
here](config/functions_categories_and_policies.md).


### Visualization

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
