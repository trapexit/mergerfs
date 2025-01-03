# rename-exdev

If using path preservation and a  `rename`  fails with `EXDEV`:

1. Move file from `/branch/a/b/c` to `/branch/.mergerfs_rename_exdev/a/b/c`.
2. symlink the rename's `newpath` to the moved file.

The  `target`  value is determined by the value of  `rename-exdev`.

* `rename-exdev=passthrough`: Return `EXDEV` as normal.
* `rename-exdev=rel-symlink`: A relative path from the  `newpath`.
* `rename-exdev=abs-symlink`: An absolute value using the mergerfs
  mount point.
* Defaults to `passthrough`.

**NOTE:** It is possible that some applications check the file they
rename. In those cases it is possible it will error or complain.

**NOTE:** The reason `abs-symlink` is not split into two like
`link-exdev` is due to the complexities in managing absolute base
symlinks when multiple `oldpaths` exist.
