# rename-exdev

If using path preservation and a  `rename`  fails with `EXDEV`:

1. Move file from `/mnt/hdd/disk0/a/b/c` to
   `/mnt/hdd/disk0/.mergerfs_rename_exdev/a/b/c`.
2. symlink the rename's `newpath` to the moved file.

The  `target`  value is determined by the value of  `rename-exdev`.

* `rename-exdev=passthrough`: Return `EXDEV` as normal.
* `rename-exdev=rel-symlink`: A relative path from the  `newpath`.
* `rename-exdev=abs-symlink`: An absolute value using the mergerfs
  mount point.
* Defaults to `passthrough`.

The staging directory is created on demand, including for files at the
pool root. Relative targets are calculated within the pool's relative
path namespace.

Replacement symlinks retain the UID and GID of the first source inode
selected by the rename action policy, read before moving the source and
without following source symlinks. Creation uses daemon credentials,
then sets the source ownership; it does not use the rename request's
UID/GID, which may be invalid when `allow-idmap` is enabled. A newly
created replacement is removed if its ownership cannot be set. If
replacement creation fails, mergerfs attempts to restore the original
source paths.

**NOTE:** It is possible that some applications check the file they
rename. In those cases it is possible it will error or complain.

**NOTE:** The reason `abs-symlink` is not split into two like
`link-exdev` is due to the complexities in managing absolute base
symlinks when multiple `oldpaths` exist.
