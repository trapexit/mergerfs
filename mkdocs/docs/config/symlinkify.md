# symlinkify

* `symlinkify=true|false`
* Defaults to `false`.

Due to the levels of indirection introduced by mergerfs and the
underlying technology FUSE there can be varying levels of performance
degradation. This feature will turn non-directories which are not
writable into symlinks to the original file found by the `readlink`
policy after the mtime and ctime are older than the timeout.

**WARNING:** The current implementation has a known issue in which if
the file is open and being used when the file is converted to a
symlink then the application which has that file open will receive an
error when using it. This is unlikely to occur in practice but is
something to keep in mind.

**WARNING:** Some backup solutions, such as CrashPlan, do not backup
the target of a symlink. If using this feature it will be necessary to
point any backup software to the original filesystems or configure the
software to follow symlinks if such an option is
available. Alternatively, create two mounts. One for backup and one
for general consumption.
