# mergerfs - a featureful union filesystem

**mergerfs** is a [union
filesystem](https://en.wikipedia.org/wiki/Union_mount) that makes
multiple storage devices or filesystems appear as a single unified
directory. Built on [FUSE (Filesystem in
Userspace)](https://en.wikipedia.org/wiki/Filesystem_in_Userspace), it
is designed to simplify how you manage files across several
independent filesystems without the complexity, fragility, or cost of
RAID or similar storage aggregation technologies.

Think of mergerfs as a smart pooling layer: you can combine any number
of [existing
filesystems](faq/usage_and_functionality.md#can-mergerfs-be-used-with-filesystems-which-already-have-data)
— whether they are on hard drives, SSDs, network shares, or other
mounted storage — into what looks like one large filesystem, while
still maintaining direct access to each individual filesystem. Unlike
RAID, there's no rebuild process if a device fails. You only lose the
files that were on that specific filesystem. You can also add or
remove filesystems at any time without restructuring your entire pool.

mergerfs excels at cost-effective storage expansion, making it ideal
for media libraries, backups, archival data, and other write-sometimes,
read-often workloads where you need lots of space but don't want
the overhead of traditional storage technologies.

**Key advantages:**

* Mix and match filesystems of any size, type, or underlying device
* No parity calculations or rebuild times
* Add or remove filesystems on the fly
* Direct access to files on individual filesystems when needed
* Flexible policies for controlling where new files are created

For users seeking alternatives to mhddfs, unionfs, aufs, or DrivePool,
mergerfs offers a mature, actively maintained solution with extensive
configuration options and documentation. See the [project comparisons
for more comparisons.](project_comparisons.md)


## Features

* Logically combine numerous filesystems/paths into a single
  mount point (JBOFS: Just a Bunch of FileSystems)
* Combine paths of the same or different filesystems
* Ability to add or remove filesystems/paths without impacting the
  rest of the data
* Unaffected by individual filesystem failure
* Configurable file creation placement
* File IO [passthrough](config/passthrough.md) for near native IO
  performance (where supported)
* Works with filesystems of any size
* Works with filesystems of [almost any
  type](faq/compatibility_and_integration.md#what-filesystems-can-be-used-as-branches)
* Ignore read-only filesystems when creating files
* [Hard links](faq/technical_behavior_and_limitations.md#do-hard-links-work)
* Hard link [copy-on-write / CoW](config/link-cow.md)
* [Runtime configurability](runtime_interface.md)
* Support for extended attributes (xattrs)
* Support for file attributes (chattr)
* Support for POSIX ACLs


## Non-features

* RAID like redundancy (see [SnapRAID](https://www.snapraid.it) and
  [NonRAID](https://github.com/qvr/nonraid))
* LVM/RAID style block device aggregation
* Data integrity checks, snapshots, file versioning
* Read/write overlays on top of read-only filesystems (like OverlayFS)
* File whiteout
* Splitting of files across branches
* Active rebalancing of content


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

The way in which mergerfs behaves is controlled by the
[config / options / settings](config/options.md). More specifically by
[policies](config/functions_categories_policies.md).


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
|   +-- file4    |                |   +-- file4
|                |                |
|                +-- /dir3        +-- /dir3
|                |   |            |   |
|                |   +-- file5    |   +-- file5
|                |                |
+-- file6        |                +-- file6
+-- file7        +-- file7        +-- file7
```

## Getting Started

Head to the [quick start guide](quickstart.md).


## About This Documentation

* Like the software the documentation changes over time. Ensure that
  you are reading the documentation related to the version of the
  software you are using.
* The documentation is explicit, literal, and reasonably thorough. If
  a suspected feature is not mentioned it does not exist. Do not read
  into the wording. What is described is how it functions. If you feel
  like something is not explained sufficiently or missing please [ask
  in one of the supported forums](support.md#contact-issue-submission)
  and the docs will be updated.
* The search feature of MkDocs is not great. Searching for "literal
  strings" will generally not work. Alernative solutions are being
  investigated.
