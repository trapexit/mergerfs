# statfs / statvfs

* `statfs=base`: Aggregate details from all branches using their base directory.
* `statfs=full`: Aggregate details using the full path of the file
  requested. Limiting it to only branches where the file exists.
* Defaults to `base`. 

[statvfs](http://linux.die.net/man/2/statvfs) normalizes the source
filesystems based on the fragment size and sums the number of adjusted
blocks and inodes. This means you will see the combined space of all
sources. Total, used, and free. The sources however are dedupped based
on the filesystem so multiple sources on the same drive will not result in
double counting its space. Other filesystems mounted further down the tree
of the branch will not be included when checking the mount's stats.

## statfs_ignore

Modifies how `statfs` works. Will cause it to ignore branches of a
certain mode.

* `statfs_ignore=none`: Include all branches.
* `statfs_ignore=ro`: Ignore available space for branches mounted as
  read-only or have a mode `RO` or `NC`.
* `statfs_ignore=nc`: Ignore available space for branches with a mode
  of `NC`.
* Defaults to `none`.
