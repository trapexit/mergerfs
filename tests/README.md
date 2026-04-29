# POSIX Soft Conformance Suite

This directory contains integration tests that exercise mergerfs FUSE operations
and compare behavior against equivalent native syscalls on a local filesystem.

The suite is "soft POSIX": it focuses on practical parity for return values,
`errno`, and key side effects, including common error paths.

Run with:

```bash
python3 tests/run-tests /mnt/mergerfs/
```

## Coverage Matrix

- `access` -> `TEST_posix_access`
- `chmod` / `fchmod` -> `TEST_posix_chmod`, `TEST_use_fchmod_after_unlink`
- `chown` / `fchown` -> `TEST_posix_chown`, `TEST_use_fchown_after_unlink`
- `create` / `mknod` -> `TEST_posix_create_mknod`
- `getattr` / `fgetattr` -> `TEST_posix_getattr_fgetattr`, `TEST_use_fstat_after_unlink`
- `open` / `read` / `write` -> `TEST_posix_open_read_write`, `TEST_o_direct`
- `flush` / `release` / `fsync` / `fsyncdir` -> `TEST_posix_fsync_flush`
- `release` close semantics -> `TEST_posix_release`
- `releasedir` close semantics -> `TEST_posix_releasedir`
- `truncate` / `ftruncate` -> `TEST_posix_truncate_ftruncate`, `TEST_use_ftruncate_after_unlink`
- `utimens` / `futimens` -> `TEST_posix_utimens`, `TEST_use_futimens_after_unlink`
- `fallocate` -> `TEST_use_fallocate_after_unlink`
- `copy_file_range` -> `TEST_posix_copy_file_range`
- `ioctl` -> `TEST_posix_ioctl`
- `poll` -> `TEST_posix_poll`
- `tmpfile` (`O_TMPFILE`) -> `TEST_posix_tmpfile`
- `bmap` (via `FIBMAP` ioctl parity) -> `TEST_posix_bmap`
- `readlink` -> `TEST_readlink_semantics`
- `link` / `symlink` -> `TEST_posix_link_symlink`
- `mkdir` / `rmdir` -> `TEST_posix_mkdir_rmdir`
- `unlink` / `rename` -> `TEST_posix_unlink_rename`, `TEST_unlink_rename`
- `statfs` -> `TEST_posix_statfs`
- `statx` -> `TEST_posix_statx`
- `syncfs` -> `TEST_posix_syncfs`
- `flock` / `lock` -> `TEST_posix_locking`
- `setxattr` / `getxattr` / `listxattr` / `removexattr` -> `TEST_posix_xattr`
- xattr object/flag matrix (`file`, `dir`, symlink `l*`, create/replace) -> `TEST_posix_xattr_matrix`
- xattr mode runtime toggles (`passthrough`/`noattr`/`nosys`) -> `TEST_cfg_xattr_modes`
- statfs mode and statfs-ignore runtime toggles -> `TEST_cfg_statfs_ignore`
- EXDEV fallback runtime toggles for link/rename -> `TEST_cfg_link_rename_exdev`
- `readdir` (+ seek/tell behavior) -> `TEST_posix_readdir`
- `readdir_plus` semantic parity (list+stat and readdir policy toggles) -> `TEST_posix_readdir_plus`
- lifecycle (`init`/`destroy`) harness check -> `TEST_mount_lifecycle`
- hidden temp exposure checks -> `TEST_no_fuse_hidden`

## Error Conditions Emphasized

Across tests, parity checks include combinations of:

- `ENOENT`
- `ENOTDIR`
- `EEXIST`
- `ENOTEMPTY`
- `EISDIR`
- `EBADF`
- privilege-sensitive outcomes (for example `EPERM`) where applicable

## Notes

- Some behavior is runtime/environment dependent (kernel, mount options,
  uid/gid, capabilities). Tests compare mergerfs to native behavior in the same
  runtime to keep assertions robust.
- Tests that cannot exercise a behavior in the current environment may exit 77
  and are reported as `SKIP` by `tests/run-tests`.
- The xattr test uses a `user.*` key and assumes user xattrs are enabled.
