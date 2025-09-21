# mergerfs - a featureful union filesystem

**mergerfs** is a
[FUSE](https://en.wikipedia.org/wiki/Filesystem_in_Userspace) based
[union filesystem](https://en.wikipedia.org/wiki/Union_mount) geared
towards simplifying storage and management of files across numerous
commodity storage devices. It is similar to [**mhddfs**, **unionfs**,
**aufs**, **DrivePool**,
etc.](https://trapexit.github.io/mergerfs/latest/project_comparisons/)


## Features

* Logically combine numerous filesystems/paths into a single
  mount point (JBOFS: Just a Bunch of FileSystems)
* Combine paths of the same or different filesystems
* Ability to add or remove filesystems/paths without impacting the
  rest of the data
* Unaffected by individual filesystem failure
* Configurable file selection and creation placement
* File IO
  [passthrough](https://trapexit.github.io/mergerfs/latest/config/passthrough/)
  for near native IO performance (where supported)
* Works with filesystems of any size
* Works with filesystems of [almost any
  type](https://trapexit.github.io/mergerfs/latest/faq/compatibility_and_integration/#what-filesystems-can-be-used-as-branches)
* Ignore read-only filesystems when creating files
* Hard link [copy-on-write /
  CoW](https://trapexit.github.io/mergerfs/latest/config/link_cow/)
* [Runtime configurable](https://trapexit.github.io/mergerfs/latest/runtime_interface/)
* Support for extended attributes (xattrs)
* Support for file attributes (chattr)
* Support for POSIX ACLs


## Non-features

* Read/write overlay on top of read-only filesystem like OverlayFS
* File whiteout
* RAID like parity calculation (see [SnapRAID](https://www.snapraid.it))
* Redundancy
* Splitting of files across branches


## How it works

mergerfs logically merges multiple filesystem paths together. Not
block devices, not filesystem mounts, just paths. It acts as a proxy
to the underlying filesystem paths. Combining the behaviors of some
functions and being a selector for others.

When the contents of a directory are requested mergerfs combines the
list of files from each directory, deduplicating entries, and returns
that list.

When a file or directory is created a policy is first run to determine
which branch will be selected for the creation.

For functions which change attributes or remove the file the behavior
may be applied to all instances found.

Read more about [policies
here](https://trapexit.github.io/mergerfs/latest/config/functions_categories_policies/).


### Visualization

```
A         +      B        =       C
/disk1           /disk2           /merged
|                |                |
+-- /dir1        +-- /dir1        +-- /dir1
|   |            |   |            |   |
|   +-- file1    |   |            |   +-- file1
|                |   +-- file2    |   +-- file2
|                |   +-- file3    |   +-- file3
|                |                |
+-- /dir2        |                +-- /dir2
|   |            |                |   |
|   *-- file4    |                |   +-- file4
|                |                |
|                +-- /dir3        +-- /dir3
|                |   |            |   |
|                |   +-- file5    |   +-- file5
|                |                |
+-- file6        |                +-- file6
+-- file7        +-- file7        +-- file7
```


## QuickStart

https://trapexit.github.io/mergerfs/latest/quickstart/


## Documentation

https://trapexit.github.io/mergerfs


## Support

https://trapexit.github.io/mergerfs/latest/support/


## Sponsorship and Donations

[https://github.com/trapexit/support](https://github.com/trapexit/support)

Development and support of a project like mergerfs requires a
significant amount of time and effort. The software is released under
the very liberal [ISC](https://opensource.org/license/isc-license-txt)
license and is therefore free to use for personal or commercial uses.

If you are a non-commercial user and find mergerfs and its support valuable
and would like to support the project financially it would be very
much appreciated.

If you are using mergerfs commercially please consider sponsoring the
project to ensure it continues to be maintained and receive
updates. If custom features are needed feel free to [contact me
directly](mailto:support@spawn.link).
